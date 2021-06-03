#include "spawn_server.h"
#include "rpc_server.h"
#include "mem_alloc.h"

#include <spawn/process_manager.h>


#include <aos/cache.h>
#include <aos/default_interfaces.h>
#include <aos/aos_datachan.h>
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
    
    err = aos_rpc_call(get_ns_rpc(),NS_GET_PID,&urpc_init[6]);
    ON_ERR_RETURN(err);

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

static void handle_binding(struct aos_rpc *r, struct capref ep, struct capref remote_in, struct capref *remote_out) {
    //debug_printf("in handle_binding\n");
    r->channel.lmp.remote_cap = ep;
}


errval_t spawn_new_domain(const char *mod_name, int argc, char **argv, domainid_t *new_pid,
                          struct capref spawner_ep, struct capref child_stdout_cap, struct capref child_stdin_cap, struct spawninfo **ret_si)
{
    errval_t err;
    struct spawninfo *si = spawn_create_spawninfo();
    
    domainid_t *pid = &si->pid;
    struct aos_rpc *rpc = &si->rpc;

    aos_rpc_set_interface(rpc, get_init_interface(), INIT_IFACE_N_FUNCTIONS, malloc(INIT_IFACE_N_FUNCTIONS * sizeof(void *)));
    initialize_rpc_handlers(rpc);


    if (capref_is_null(spawner_ep)) {
        struct aos_rpc *disp_rpc = &si->disp_rpc;
        aos_rpc_init_lmp(disp_rpc, NULL_CAP, NULL_CAP, NULL, NULL);
        aos_rpc_set_interface(disp_rpc, get_dispatcher_interface(), DISP_IFACE_N_FUNCTIONS, malloc(DISP_IFACE_N_FUNCTIONS * sizeof(void *)));
        aos_rpc_register_handler(disp_rpc, DISP_IFACE_BINDING, handle_binding);

        si->spawner_ep_cap = disp_rpc->channel.lmp.local_cap;
    }
    else {
        si->spawner_ep_cap = spawner_ep;
    }

    si->child_stdout_cap = child_stdout_cap;
    si->child_stdin_cap = child_stdin_cap;
    //initialize_rpc_handlers(rpc);
    if (argv == NULL || argc == 0) {
        err = spawn_load_by_name((char*) mod_name, si, pid);
        ON_ERR_RETURN(err);
        // si -> binary_name = (char*) mod_name;
    }
    else {
        err = spawn_setup_module_by_name(mod_name, si);
        if (err_is_fail(err)) {
            return err;
        }
        si->binary_name = argv[0];
        err = spawn_setup_dispatcher(argc, (const char * const *)argv, si, pid);
        ON_ERR_RETURN(err);
        err = spawn_invoke_dispatcher(si);
        ON_ERR_RETURN(err);
    }
    //



    if (new_pid != NULL) {
        *new_pid = *pid;
    }
    err = lmp_chan_register_recv(&rpc->channel.lmp, get_default_waitset(), MKCLOSURE(&aos_rpc_on_lmp_message, &rpc));
    

    if (err_is_fail(err) && err == LIB_ERR_CHAN_ALREADY_REGISTERED) {
        // not too bad, already registered
    }
    if (ret_si != NULL) {
        *ret_si = si;
    }

    return SYS_ERR_OK;
}


errval_t spawn_lpuart_driver(const char *mod_name, struct spawninfo **ret_si, struct capref in, struct capref out)
{
    errval_t err;
    struct spawninfo *si = spawn_create_spawninfo();

    domainid_t *pid = &si->pid;
    struct aos_rpc *rpc = &si->rpc;

    aos_rpc_set_interface(rpc, get_init_interface(), INIT_IFACE_N_FUNCTIONS, malloc(INIT_IFACE_N_FUNCTIONS * sizeof(void *)));
    initialize_rpc_handlers(rpc);


    struct aos_rpc *disp_rpc = &si->disp_rpc;


    aos_rpc_init_lmp(disp_rpc, NULL_CAP, NULL_CAP, NULL, NULL);


    aos_rpc_set_interface(disp_rpc, get_dispatcher_interface(), DISP_IFACE_N_FUNCTIONS, malloc(DISP_IFACE_N_FUNCTIONS * sizeof(void *)));
    aos_rpc_register_handler(disp_rpc, DISP_IFACE_BINDING, handle_binding);
    debug_printf("disp_rpc: %p\n", disp_rpc);
    debug_printf("handle_binding: %p\n", handle_binding);

    si->spawner_ep_cap = disp_rpc->channel.lmp.local_cap;

    si->child_stdin_cap = in;
    si->child_stdout_cap = out;

    spawn_setup_by_name((char*) mod_name, si, pid);
    /*err = lmp_chan_register_recv(&rpc->channel.lmp, get_default_waitset(), MKCLOSURE(&aos_rpc_on_lmp_message, &rpc));
    if (err_is_fail(err) && err == LIB_ERR_CHAN_ALREADY_REGISTERED) {
        // not too bad, already registered
    }*/
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
    ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_RETYPE);


    struct capref irq = (struct capref) {
        .cnode = child_taskcn,
        .slot = TASKCN_SLOT_IRQ
    };
    err = cap_copy(irq, cap_irq);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_COPY);

    err = spawn_invoke_dispatcher(si);
    ON_ERR_RETURN(err);

    if (ret_si != NULL) {
        *ret_si = si;
    }
    return SYS_ERR_OK;
}

errval_t spawn_enet_driver(const char *mod_name, struct spawninfo **ret_si) {
    errval_t err;
    struct spawninfo *si = spawn_create_spawninfo();

    domainid_t *pid = &si->pid;
    struct aos_rpc *rpc = &si->rpc;

    aos_rpc_set_interface(rpc, get_init_interface(), INIT_IFACE_N_FUNCTIONS, malloc(INIT_IFACE_N_FUNCTIONS * sizeof(void *)));
    initialize_rpc_handlers(rpc);


    struct aos_rpc *disp_rpc = &si->disp_rpc;


    aos_rpc_init_lmp(disp_rpc, NULL_CAP, NULL_CAP, NULL, NULL);
    aos_rpc_set_interface(disp_rpc, get_dispatcher_interface(), DISP_IFACE_N_FUNCTIONS, malloc(DISP_IFACE_N_FUNCTIONS * sizeof(void *)));
    aos_rpc_register_handler(disp_rpc, DISP_IFACE_BINDING, handle_binding);
    //debug_printf("disp_rpc: %p\n", disp_rpc);
    //debug_printf("handle_binding: %p\n", handle_binding);

    si->spawner_ep_cap = disp_rpc->channel.lmp.local_cap;

    spawn_setup_by_name((char*) mod_name, si, pid);
    /*err = lmp_chan_register_recv(&rpc->channel.lmp, get_default_waitset(), MKCLOSURE(&aos_rpc_on_lmp_message, &rpc));
    if (err_is_fail(err) && err == LIB_ERR_CHAN_ALREADY_REGISTERED) {
        // not too bad, already registered
    }*/

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

    struct capref irq = (struct capref) {
        .cnode = child_taskcn,
        .slot = TASKCN_SLOT_IRQ
    };
    err = cap_copy(irq, cap_irq);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_COPY);

    err = spawn_invoke_dispatcher(si);
    ON_ERR_RETURN(err);

    if (ret_si != NULL) {
        *ret_si = si;
    }

    return SYS_ERR_OK;
}

/* errval_t spawn_enet_driver(const char *mod_name, struct spawninfo **ret_si) { */
/*     errval_t err; */
/*     struct spawninfo *si = spawn_create_spawninfo(); */

/*     domainid_t *pid = &si->pid; */
/*     struct aos_rpc *rpc = &si->rpc; */


/*     aos_rpc_set_interface(rpc, get_dispatcher_interface(), DISP_IFACE_N_FUNCTIONS, malloc(DISP_IFACE_N_FUNCTIONS * sizeof(void *))); */
/*     initialize_initiate_handler(rpc); */

/*     aos_rpc_register_handler(rpc, INIT_IFACE_GET_RAM, handle_get_ram); */

/*     struct lmp_endpoint *spawner_ep; */
/*     struct capref spawner_ep_cap; */
/*     endpoint_create(LMP_RECV_LENGTH, &spawner_ep_cap, &spawner_ep); */

/*     err = spawn_load_by_name((char*) mod_name, si, pid); */
/*     if (err_is_fail(err)) { */
/*         DEBUG_ERR(err, "fuck no\n"); */
/*         return err; */
/*     } */

/*     err = lmp_chan_register_recv(&rpc->channel.lmp, get_default_waitset(), MKCLOSURE(&aos_rpc_on_lmp_message, &rpc)); */
/*     if (err_is_fail(err) && err == LIB_ERR_CHAN_ALREADY_REGISTERED) { */
/*         // not too bad, already registered */
/*     } */

/*     struct cnoderef child_taskcn = { */
/*         .croot = get_cap_addr(si->rootcn), */
/*         .cnode = ROOTCN_SLOT_ADDR(ROOTCN_SLOT_TASKCN), */
/*         .level = CNODE_TYPE_OTHER */
/*     }; */

/*     struct capref dev_frame = (struct capref) { */
/*         .cnode = cnode_task, */
/*         .slot = TASKCN_SLOT_DEV */
/*     }; */
/*     struct capref child_dev_frame = (struct capref) { */
/*         .cnode = child_taskcn, */
/*         .slot = TASKCN_SLOT_DEV */
/*     }; */
/*     /\* struct capref child_dev_frame2 = (struct capref) { *\/ */
/*     /\*     .cnode = child_taskcn, *\/ */
/*     /\*     .slot = TASKCN_SLOT_BOOTINFO *\/ */
/*     /\* }; *\/ */

/*     // write capabilities to access the enet driver into the child  */
/*     size_t source_addr = get_phys_addr(dev_frame); */
/*     err = cap_retype(child_dev_frame, dev_frame, IMX8X_ENET_BASE - source_addr, ObjType_DevFrame, IMX8X_ENET_SIZE, 1); */
/*     ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_RETYPE); */

/*     struct capref irq = (struct capref) { */
/*         .cnode = child_taskcn, */
/*         .slot = TASKCN_SLOT_IRQ */
/*     }; */
/*     err = cap_copy(irq, cap_irq); */
/*     ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_COPY); */

/*     if (ret_si != NULL) { */
/*         *ret_si = si; */
/*     } */

/*     return SYS_ERR_OK; */
/* } */

errval_t spawn_filesystem(const char *mod_name, struct spawninfo **ret_si)
{
    errval_t err;
    struct spawninfo *si = spawn_create_spawninfo();

    domainid_t *pid = &si->pid;
    struct aos_rpc *rpc = &si->rpc;

    aos_rpc_set_interface(rpc, get_init_interface(), INIT_IFACE_N_FUNCTIONS, malloc(INIT_IFACE_N_FUNCTIONS * sizeof(void *)));
    initialize_rpc_handlers(rpc);
    struct aos_rpc *disp_rpc = &si->disp_rpc;

    aos_rpc_init_lmp(disp_rpc, NULL_CAP, NULL_CAP, NULL, NULL);
    aos_rpc_set_interface(disp_rpc, get_dispatcher_interface(), DISP_IFACE_N_FUNCTIONS, malloc(DISP_IFACE_N_FUNCTIONS * sizeof(void *)));
    aos_rpc_register_handler(disp_rpc, DISP_IFACE_BINDING, handle_binding);
    //debug_printf("disp_rpc: %p\n", disp_rpc);
    //debug_printf("handle_binding: %p\n", handle_binding);

    si->spawner_ep_cap = disp_rpc->channel.lmp.local_cap;

    err = spawn_setup_by_name((char*) mod_name, si, pid);
    ON_ERR_RETURN(err);

    struct cnoderef child_taskcn = {
        .croot = get_cap_addr(si->rootcn),
        .cnode = CPTR_TASKCN_BASE,
        .level = CNODE_TYPE_OTHER
    };
    struct cnoderef child_argcn = {
            .croot = get_cap_addr(si->rootcn),
            .cnode = CPTR_ARGCN_BASE,
            .level = CNODE_TYPE_OTHER
    };

    struct capref dev_frame = (struct capref) {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_DEV
    };
    struct capref child_argcn_frame = (struct capref) {
        .cnode = child_argcn,
        .slot = ARGCN_SLOT_DEV_0
    };
    /*
    struct capref child_dev_frame = (struct capref) {
        .cnode = child_taskcn,
        .slot = TASKCN_SLOT_BOOTINFO
    };
    */

    // write capabilities to access the sdhc driver into the child
    size_t source_addr = get_phys_addr(dev_frame);
    err = cap_retype(child_argcn_frame, dev_frame, IMX8X_SDHC2_BASE - source_addr, ObjType_DevFrame, IMX8X_SDHC_SIZE, 1);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_RETYPE);

    /*
    source_addr = get_phys_addr(dev_frame);
    err = cap_retype(child_dev_frame, dev_frame, IMX8X_GIC_DIST_BASE - source_addr, ObjType_DevFrame, IMX8X_GIC_DIST_SIZE, 1);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_RETYPE);
    */
    struct capref irq = (struct capref) {
        .cnode = child_taskcn,
        .slot = TASKCN_SLOT_IRQ
    };
    err = cap_copy(irq, cap_irq);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_COPY);

    err = spawn_invoke_dispatcher(si);
    ON_ERR_RETURN(err);

    if (ret_si != NULL) {
        *ret_si = si;
    }

    return SYS_ERR_OK;
}
