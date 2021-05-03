    /**
 * \file
 * \brief init process for child spawning
 */

/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/morecore.h>
#include <aos/paging.h>
#include <aos/waitset.h>
#include <aos/aos_rpc.h>
#include <mm/mm.h>
#include <grading.h>
#include <aos/core_state.h>
#include <aos/systime.h>
#include <aos/threads.h>

#include <aos/dispatch.h>
#include <aos/dispatcher_arch.h>
#include <aos/curdispatcher_arch.h>
#include <barrelfish_kpi/dispatcher_shared.h>
#include <barrelfish_kpi/startup_arm.h>


#include <spawn/spawn.h>
#include <spawn/process_manager.h>
#include "spawn_server.h"
#include "mem_alloc.h"
#include "rpc_server.h"
#include "test.h"

#include <aos/kernel_cap_invocations.h>


// #include <aos/curdispatcher_arch.h>


coreid_t my_core_id;

static int bsp_main(int argc, char *argv[])
{
    errval_t err;

    // Grading
    grading_setup_bsp_init(argc, argv);

    // First argument contains the bootinfo location, if it's not set
    bi = (struct bootinfo *) strtol(argv[1], NULL, 10);
    assert(bi);

    // TODO: initialize mem allocator, vspace management here
    err = initialize_ram_alloc();
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "/>/> Error: Initialize_ram_alloc");
    }

    err = init_terminal_state();

    //run_init_tests(my_core_id);

    // Grading
    grading_test_early();

    // TODO: Spawn system processes, boot second core etc. here
    
    spawn_new_core(my_core_id + 1);
    //spawn_new_domain("performance_tester", NULL);


    aos_rpc_call(core_channels[1], AOS_RPC_FOREIGN_SPAWN, "performance_tester");
    aos_rpc_call(core_channels[1], AOS_RPC_FOREIGN_SPAWN, "memeater");
    /*size_t counter = 0;
    while(1) {
        if (counter % (1 << 28) == 0){
            aos_rpc_call(core_channels[1], AOS_RPC_SEND_NUMBER, counter);
        }
        counter++;
    }*/


    // Grading
    grading_test_late();

    debug_printf("Message handler loop\n");
    // Hang around
    struct waitset *default_ws = get_default_waitset();
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

    return EXIT_SUCCESS;
}



static errval_t init_foreign_core(void){
    errval_t err;
    uint64_t *urpc_init = (uint64_t*) MON_URPC_VBASE;
        debug_printf("Bootinfo base: %lx, bootinfo size: %lx\n",urpc_init[0],urpc_init[1]);
    debug_printf("Ram base: %lx, Ram size: %lx\n",urpc_init[2],urpc_init[3]);
    struct capref bootinfo_cap = {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_BOOTINFO,
    };
    struct capref core_ram = {
        .cnode = cnode_super,
        .slot = 0,
    };

    err = frame_forge(bootinfo_cap, urpc_init[0], urpc_init[1], 0);
    ON_ERR_RETURN(err);
    
    err = paging_map_frame_complete(get_current_paging_state(),(void **) &bi,bootinfo_cap,NULL,NULL);
    ON_ERR_RETURN(err);
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"Failed to map bootinfo struct");
    // }

    assert(bi && "Boot info in appmain is NULL");


    err = ram_forge(core_ram, urpc_init[2], urpc_init[3], disp_get_current_core_id());
    ON_ERR_RETURN(err);
    err = initialize_ram_alloc_foreign(core_ram);
    
    ON_ERR_RETURN(err);

    const int nBenches = 100;

    for (int i = 0; i < 3; i++) {
        uint64_t before = systime_now();
        for (int j = 0; j < nBenches; j++) {
            struct capref thrown_away;
            err  = ram_alloc(&thrown_away, BASE_PAGE_SIZE);
            // debug_printf("Allocated ram!\n");
            if(err_is_fail(err)){
                DEBUG_ERR(err,"Failed to allocate ram in benchmarkmm\n");
            }
        }
        uint64_t end = systime_now();

        debug_printf("measurement %d took: %ld\n", i, systime_to_ns(end - before));
    }

    struct capref mc = {
        .cnode = cnode_root,
        .slot = ROOTCN_SLOT_MODULECN
    };
    err = cnode_create_raw(mc, NULL, ObjType_L2CNode, L2_CNODE_SLOTS, NULL);
    
    err = frame_forge(cap_mmstrings, urpc_init[4], urpc_init[5], 0);
    ON_ERR_RETURN(err);
    
    for(int i = 0; i < bi -> regions_length;++i) {
        
        if(bi -> regions[i].mr_type == RegionType_Module){
            struct capref module_cap = {
                .cnode = cnode_module,
                .slot = bi -> regions[i].mrmod_slot
            };
            size_t size = bi->regions[i].mrmod_size;
            debug_printf("Trying to forge cap: %ld bytes\n", size);
            err = frame_forge(module_cap, bi->regions[i].mr_base, ROUND_UP(size, BASE_PAGE_SIZE), disp_get_current_core_id()); 
            // ON_ERR_RETURN(err);
            if(err_is_fail(err)){
                DEBUG_ERR(err,"Failed to forge cap for modules held by bootinfo\n");
            }
        }
    }

    //spawn_memeater();
    //

    init_core_channel(0, (lvaddr_t) urpc_init);

    return SYS_ERR_OK;
}










static int app_main(int argc, char *argv[])
{
    // Implement me in Milestone 5
    // Remember to call
    // - grading_setup_app_init(..);
    // - grading_test_early();
    // - grading_test_late();
    
    errval_t err;

    debug_printf("Hello from core: %d!\n",disp_get_current_core_id());
    err = init_foreign_core();
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to initialize ram and bootinfo for new core core\n");
    }
    
    //run_init_tests(my_core_id);

    grading_setup_app_init(bi);

    grading_test_early();

    grading_test_late();

    debug_printf("Message handler loop\n");
    // Hang around
    struct waitset *default_ws = get_default_waitset();
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

    thread_exit(0);
    return SYS_ERR_OK;
}


int main(int argc, char *argv[])
{
    errval_t err;


    /* Set the core id in the disp_priv struct */
    err = invoke_kernel_get_core_id(cap_kernel, &my_core_id);
    assert(err_is_ok(err));
    disp_set_core_id(my_core_id);

    debug_printf("init: on core %" PRIuCOREID ", invoked as:", my_core_id);
    for (int i = 0; i < argc; i++) {
        printf(" %s", argv[i]);
    }
    printf("\n");
    fflush(stdout);


    if (my_core_id == 0)
        return bsp_main(argc, argv);
    else
        return app_main(argc, argv);
}
