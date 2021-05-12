#include "spawn_server.h"
#include "rpc_server.h"
#include "mem_alloc.h"

#include <spawn/process_manager.h>


#include <aos/cache.h>
#include <aos/default_interfaces.h>
#include <aos/coreboot.h>


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

errval_t spawn_new_domain(const char *mod_name, domainid_t *new_pid)
{
    struct spawninfo *si = spawn_create_spawninfo();

    domainid_t *pid = &si->pid;
    struct aos_rpc *rpc = &si->rpc;

    aos_rpc_set_interface(rpc, get_init_interface(), INIT_IFACE_N_FUNCTIONS, malloc(INIT_IFACE_N_FUNCTIONS * sizeof(void *)));
    initialize_initiate_handler(rpc);
    aos_rpc_register_handler(rpc, INIT_IFACE_GET_RAM, handle_get_ram);

    //initialize_rpc_handlers(rpc);
    spawn_load_by_name((char*) mod_name, si, pid);

    char buf[128];
    debug_print_cap_at_capref(buf, 128, si->rpc.channel.lmp.remote_cap);
    debug_printf("child ep: %s", buf);
    //
    //initialize_initiate_handler(rpc);

    if (new_pid != NULL) {
        *new_pid = *pid;
    }
    errval_t err = lmp_chan_register_recv(&rpc->channel.lmp, get_default_waitset(), MKCLOSURE(&aos_rpc_on_lmp_message, &rpc));
    if (err_is_fail(err) && err == LIB_ERR_CHAN_ALREADY_REGISTERED) {
        // not too bad, already registered
    }

    return SYS_ERR_OK;
}
