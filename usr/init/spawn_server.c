#include "spawn_server.h"
#include "rpc_server.h"
#include "mem_alloc.h"

#include <spawn/process_manager.h>


#include <aos/cache.h>
#include <aos/default_interfaces.h>
#include <aos/coreboot.h>

#include <maps/imx8x_map.h>


errval_t spawn_new_core(coreid_t core)
{
    errval_t err;
    const char *boot_driver = "boot_armv8_generic";
    const char *cpu_driver = "cpu_imx8x";
    const char *init = "init";
    struct capref urpc_cap;
    size_t urpc_cap_size;
    err  = frame_alloc(&urpc_cap,BASE_PAGE_SIZE,&urpc_cap_size);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to allocate frame for urpc channel\n");
    }

    struct frame_identity urpc_frame_id = (struct frame_identity) {
        .base = get_phys_addr(urpc_cap),
        .bytes = urpc_cap_size,
        .pasid = disp_get_core_id()
    };
    char *urpc_data = NULL;
    paging_map_frame_complete(get_current_paging_state(), (void **) &urpc_data, urpc_cap, NULL, NULL);


    //Write address of bootinfo into urpc frame to setup 
    //================================================
    struct capref bootinfo_cap = {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_BOOTINFO,
    };
    struct capref core_ram;
    err = ram_alloc(&core_ram,1L << 28);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to allcoate ram for new core\n");
    }
    uint64_t* urpc_init = (uint64_t *) urpc_data;
    debug_printf("Here is bi in bsp: %lx \n", bi);
    debug_printf("Bootinfo base: %lx, bootinfo size: %lx\n",get_phys_addr(bootinfo_cap),BOOTINFO_SIZE);
    debug_printf("Core ram base: %lx, core ram size: %lx\n",get_phys_addr(core_ram),get_phys_size(core_ram));
    urpc_init[0] = get_phys_addr(bootinfo_cap);
    urpc_init[1] = BOOTINFO_SIZE;
    urpc_init[2] = get_phys_addr(core_ram);
    urpc_init[3] = get_phys_size(core_ram);
    urpc_init[4] = get_phys_addr(cap_mmstrings);
    urpc_init[5] = get_phys_size(cap_mmstrings);

    cpu_dcache_wbinv_range((vm_offset_t) urpc_data, BASE_PAGE_SIZE);

    coreid_t coreid = core;
    err = coreboot(coreid,boot_driver,cpu_driver,init,urpc_frame_id);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to boot core");
    }

    err = init_core_channel(coreid, (lvaddr_t) urpc_init);
    ON_ERR_RETURN(err);

    return SYS_ERR_OK;
}

static void handle_get_ram(struct aos_rpc *rpc, size_t size, size_t alignment, struct capref *ramcap, size_t *retsize) {
    ram_alloc_aligned(ramcap, size, alignment);
    *retsize = size;
}

errval_t spawn_new_domain(const char *mod_name, domainid_t *new_pid, struct capref spawner_ep_cap, struct spawninfo **ret_si)
{
    struct spawninfo *si = spawn_create_spawninfo();

    domainid_t *pid = &si->pid;
    struct aos_rpc *rpc = &si->rpc;

    aos_rpc_set_interface(rpc, get_init_interface(), INIT_IFACE_N_FUNCTIONS, malloc(INIT_IFACE_N_FUNCTIONS * sizeof(void *)));
    initialize_initiate_handler(rpc);
    aos_rpc_register_handler(rpc, INIT_IFACE_GET_RAM, handle_get_ram);

    if (capref_is_null(spawner_ep_cap)) {
        struct lmp_endpoint *spawner_ep;
        endpoint_create(LMP_RECV_LENGTH, &spawner_ep_cap, &spawner_ep);
    }

    si->spawner_ep_cap = spawner_ep_cap;


    //initialize_rpc_handlers(rpc);
    spawn_load_by_name((char*) mod_name, si, pid);

    /*char buf[128];
    debug_print_cap_at_capref(buf, 128, si->rpc.channel.lmp.remote_cap);
    debug_printf("child ep: %s", buf);*/
    //
    //initialize_initiate_handler(rpc);

    if (new_pid != NULL) {
        *new_pid = *pid;
    }
    errval_t err = lmp_chan_register_recv(&rpc->channel.lmp, get_default_waitset(), MKCLOSURE(&aos_rpc_on_lmp_message, &rpc));
    if (err_is_fail(err) && err == LIB_ERR_CHAN_ALREADY_REGISTERED) {
        // not too bad, already registered
    }
    if (ret_si != NULL) {
        *ret_si = si;
    }

    return SYS_ERR_OK;
}


errval_t spawn_lpuart_driver(const char *mod_name)
{
    errval_t err;
    struct spawninfo *si = spawn_create_spawninfo();

    domainid_t *pid = &si->pid;
    struct aos_rpc *rpc = &si->rpc;
    

    aos_rpc_set_interface(rpc, get_dispatcher_interface(), DISP_IFACE_N_FUNCTIONS, malloc(DISP_IFACE_N_FUNCTIONS * sizeof(void *)));
    initialize_initiate_handler(rpc);
    aos_rpc_register_handler(rpc, INIT_IFACE_GET_RAM, handle_get_ram);
    
    struct lmp_endpoint *spawner_ep;
    struct capref spawner_ep_cap;
    endpoint_create(LMP_RECV_LENGTH, &spawner_ep_cap, &spawner_ep);

    spawn_load_by_name((char*) mod_name, si, pid);
    err = lmp_chan_register_recv(&rpc->channel.lmp, get_default_waitset(), MKCLOSURE(&aos_rpc_on_lmp_message, &rpc));
    if (err_is_fail(err) && err == LIB_ERR_CHAN_ALREADY_REGISTERED) {
        // not too bad, already registered
    }

    struct cnoderef child_taskcn = {
        .croot = get_cap_addr(si->rootcn),
        .cnode = ROOTCN_SLOT_ADDR(ROOTCN_SLOT_TASKCN),
        .level = CNODE_TYPE_OTHER
    };

    struct capref dev_frame = (struct capref) {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_DEV
    };
    struct capref child_dev_frame = (struct capref) {
        .cnode = child_taskcn,
        .slot = TASKCN_SLOT_DEV
    };
    struct capref child_dev_frame2 = (struct capref) {
        .cnode = child_taskcn,
        .slot = TASKCN_SLOT_BOOTINFO
    };

    // write capabilities to access the lpuart driver into the child 
    size_t source_addr = get_phys_addr(dev_frame);
    err = cap_retype(child_dev_frame, dev_frame, IMX8X_UART3_BASE - source_addr, ObjType_DevFrame, IMX8X_UART_SIZE, 1);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_RETYPE);

    source_addr = get_phys_addr(dev_frame);
    err = cap_retype(child_dev_frame2, dev_frame, IMX8X_GIC_DIST_BASE - source_addr, ObjType_DevFrame, IMX8X_GIC_DIST_SIZE, 1);
    DEBUG_ERR(err, "oh no\n");
    ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_RETYPE);


    struct capref irq = (struct capref) {
        .cnode = child_taskcn,
        .slot = TASKCN_SLOT_IRQ
    };
    err = cap_copy(irq, cap_irq);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_COPY);


    return SYS_ERR_OK;
}

errval_t spawn_enet_driver(const char *mod_name) {
    errval_t err;
    struct spawninfo *si = spawn_create_spawninfo();

    domainid_t *pid = &si->pid;
    struct aos_rpc *rpc = &si->rpc;
    

    aos_rpc_set_interface(rpc, get_dispatcher_interface(), DISP_IFACE_N_FUNCTIONS, malloc(DISP_IFACE_N_FUNCTIONS * sizeof(void *)));
    initialize_initiate_handler(rpc);
    aos_rpc_register_handler(rpc, INIT_IFACE_GET_RAM, handle_get_ram);
    
    struct lmp_endpoint *spawner_ep;
    struct capref spawner_ep_cap;
    endpoint_create(LMP_RECV_LENGTH, &spawner_ep_cap, &spawner_ep);

    err = spawn_load_by_name((char*) mod_name, si, pid);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "fuck no\n");
        return err;
    }

    err = lmp_chan_register_recv(&rpc->channel.lmp, get_default_waitset(), MKCLOSURE(&aos_rpc_on_lmp_message, &rpc));
    if (err_is_fail(err) && err == LIB_ERR_CHAN_ALREADY_REGISTERED) {
        // not too bad, already registered
    }

    struct cnoderef child_taskcn = {
        .croot = get_cap_addr(si->rootcn),
        .cnode = ROOTCN_SLOT_ADDR(ROOTCN_SLOT_TASKCN),
        .level = CNODE_TYPE_OTHER
    };

    struct capref dev_frame = (struct capref) {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_DEV
    };
    struct capref child_dev_frame = (struct capref) {
        .cnode = child_taskcn,
        .slot = TASKCN_SLOT_DEV
    };
    /* struct capref child_dev_frame2 = (struct capref) { */
    /*     .cnode = child_taskcn, */
    /*     .slot = TASKCN_SLOT_BOOTINFO */
    /* }; */

    // write capabilities to access the enet driver into the child 
    size_t source_addr = get_phys_addr(dev_frame);
    err = cap_retype(child_dev_frame, dev_frame, IMX8X_ENET_BASE - source_addr, ObjType_DevFrame, IMX8X_ENET_SIZE, 1);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_RETYPE);

    source_addr = get_phys_addr(dev_frame);


    struct capref irq = (struct capref) {
        .cnode = child_taskcn,
        .slot = TASKCN_SLOT_IRQ
    };
    err = cap_copy(irq, cap_irq);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_COPY);


    return SYS_ERR_OK;
}
