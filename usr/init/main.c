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
#include <aos/default_interfaces.h>
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

#include <process_manager_interface.h>


// #include <aos/curdispatcher_arch.h>


coreid_t my_core_id;
static errval_t init_foreign_core(void){

    /*struct capref epcap;
    slot_alloc(&epcap);

    struct capability ipi_ep = {
        .type = ObjType_EndPointIPI,
        .rights = CAPRIGHTS_READ_WRITE,
        .u.endpointipi = {
            .channel_id = 1,
            .notifier = (void*) 0xffff000008000000,
            .listener_core = 0
        }
    };

    invoke_monitor_create_cap((uint64_t *)&ipi_ep,
                                     get_cnode_addr(epcap),
                                     get_cnode_level(epcap),
                                     epcap.slot, 0);

    debug_printf("notifying\n");
    invoke_ipi_notify(epcap);
    //invoke_ipi_register(epcap, 4);

    //lmp_endpoint_register(ep, get_default_waitset(), MKCLOSURE(hey, ep));

    return SYS_ERR_OK;*/


    errval_t err;
    set_pm_online();

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
    assert(bi && "Boot info in appmain is NULL");


    err = ram_forge(core_ram, urpc_init[2], urpc_init[3], disp_get_current_core_id());
    ON_ERR_RETURN(err);
    err = initialize_ram_alloc_foreign(core_ram);
    
    ON_ERR_RETURN(err);



    err = start_memory_server_thread();
    ON_ERR_RETURN(err);

    set_ns_online();

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
            // debug_printf("Trying to forge cap: %ld bytes\n", size);
            err = frame_forge(module_cap, bi->regions[i].mr_base, ROUND_UP(size, BASE_PAGE_SIZE), disp_get_current_core_id()); 
            ON_ERR_RETURN(err);
        }
    }

    init_core_channel(0, (lvaddr_t) urpc_init);
    set_ns_forw_rpc(get_core_channel(0));
    

    struct aos_rpc* ns_rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
    struct capref ns_cap;
    err = slot_alloc(&ns_cap);
    ON_ERR_RETURN(err);
    size_t urpc_cap_size;
    err  = frame_alloc(&ns_cap,BASE_PAGE_SIZE,&urpc_cap_size);
    ON_ERR_RETURN(err);
    char *urpc_data = NULL;
    err = paging_map_frame_complete(get_current_paging_state(), (void **) &urpc_data, ns_cap, NULL, NULL);
    ON_ERR_RETURN(err);
    err =  aos_rpc_init_ump_default(ns_rpc,(lvaddr_t) urpc_data, BASE_PAGE_SIZE,true);//take first half as ns takes second half
    ON_ERR_RETURN(err);
    domainid_t my_pid = urpc_init[6]; 

    struct capref dummy_cap; // not useed
    err = aos_rpc_call(get_ns_forw_rpc(),INIT_REG_NAMESERVER,disp_get_core_id(),"init",ns_cap,my_pid,&dummy_cap);
    ON_ERR_RETURN(err);
    err = aos_rpc_set_interface(ns_rpc,get_nameserver_interface(),0,NULL);
    ON_ERR_RETURN(err);
    disp_set_domain_id(my_pid);
    set_ns_rpc(ns_rpc);

    
    set_ns_online();
    return SYS_ERR_OK;
}





static errval_t init_name_server(void){
    errval_t err;
    struct spawninfo *ns_si = spawn_create_spawninfo();
    domainid_t *ns_pid = &ns_si -> pid;
    err = spawn_load_by_name("nameserver",ns_si,ns_pid);
    *ns_pid = 0;
    ON_ERR_RETURN(err);


    struct aos_rpc *rpc  = &ns_si->rpc;

    aos_rpc_set_interface(rpc, get_init_interface(), INIT_IFACE_N_FUNCTIONS, malloc(INIT_IFACE_N_FUNCTIONS * sizeof(void *)));
    initialize_rpc_handlers(rpc);

    debug_printf("waiting for name server to come online...\n");
    while(!get_ns_online()){
        err = event_dispatch(get_default_waitset());
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

    // init

    domainid_t my_pid = 1;
    struct aos_rpc* ns_rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
    struct capref ns_cap;
    err = slot_alloc(&ns_cap);
    struct lmp_endpoint *name_server_ep;
    err = endpoint_create(LMP_RECV_LENGTH, &ns_cap, &name_server_ep);
    ON_ERR_RETURN(err);
    err = aos_rpc_init_lmp(ns_rpc,ns_cap,NULL_CAP,name_server_ep,get_default_waitset());
    ON_ERR_RETURN(err);
    struct capref remote_ns_cap;
    err = aos_rpc_call(rpc,INIT_REG_NAMESERVER,disp_get_core_id(),"init",ns_cap,my_pid,&remote_ns_cap);
    ON_ERR_RETURN(err);
    ns_rpc -> channel.lmp.remote_cap = remote_ns_cap;
    err = aos_rpc_set_interface(ns_rpc,get_nameserver_interface(),0,NULL);
    ON_ERR_RETURN(err);
    disp_set_domain_id(my_pid);
    set_ns_rpc(ns_rpc);
    set_ns_forw_rpc(rpc);
    // set_ns_online();

    return SYS_ERR_OK;
}




__unused
static void hey(void* arg) {
    debug_printf("we were pinged!\n");
    struct lmp_recv_buf msg;
    lmp_endpoint_recv(arg, &msg, NULL);
    lmp_endpoint_register(arg, get_default_waitset(), MKCLOSURE(hey, arg));
}








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

    err = start_memory_server_thread();
    thread_yield();
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "/>/> Error: starting memory thread");
    }

    err = init_terminal_state();
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to init terminal state\n");
    }

    err = init_name_server();
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to start nameserver\n");
    }


    spawn_new_core(1);


    /*struct capref lmp_ep;
    struct lmp_endpoint *le;

    struct capref ump_ep;

    endpoint_create(64, &lmp_ep, &le);
    err = ipi_endpoint_create(lmp_ep, &ump_ep);

    lmp_endpoint_register(le, get_default_waitset(), MKCLOSURE(hey, le));

    invoke_ipi_notify(ump_ep);*/

    struct spawninfo *term_si;
    spawn_lpuart_driver("lpuart_terminal", &term_si);

    domainid_t pid;
    struct spawninfo *josh_si;

    while (capref_is_null(term_si->disp_rpc.channel.lmp.remote_cap)) {
        err = event_dispatch(get_default_waitset());
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
    err = event_dispatch(get_default_waitset());
    err = event_dispatch(get_default_waitset());

    debug_printf("getting stdin from terminal!\n");

    struct capref testep;
    aos_rpc_call(&term_si->disp_rpc, DISP_IFACE_GET_STDIN, &testep);

    debug_printf("got stdin from terminal!\n");


    spawn_new_domain("josh", 0, NULL, &pid, NULL_CAP, testep, &josh_si);


    while (capref_is_null(josh_si->disp_rpc.channel.lmp.remote_cap)) {
        err = event_dispatch(get_default_waitset());
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
    err = event_dispatch(get_default_waitset());
    err = event_dispatch(get_default_waitset());

    struct capref josh_in;
    debug_printf("getting stdin from josh!\n");
    aos_rpc_call(&josh_si->disp_rpc, DISP_IFACE_GET_STDIN, &josh_in);
    debug_printf("got stdin from josh!\n");
    aos_rpc_call(&term_si->disp_rpc, DISP_IFACE_SET_STDOUT, josh_in);



    //struct aos_rpc *josh_rpc = &josh_si->disp_rpc;

    /*aos_rpc_set_interface(josh_rpc, get_dispatcher_interface(), DISP_IFACE_N_FUNCTIONS, malloc(DISP_IFACE_N_FUNCTIONS * sizeof(void *)));
    initialize_initiate_handler(josh_rpc);
    aos_rpc_init_lmp(josh_rpc, josh_si->spawner_ep_cap, NULL_CAP, josh_si->spawner_ep, NULL);*/

    //__unused
    //struct aos_rpc *lpuart_rpc = get_rpc_from_spawn_info(pid - 1);


    /*while (capref_is_null(josh_rpc->channel.lmp.remote_cap)) {
        err = event_dispatch(get_default_waitset());
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
    debug_printf("getting josh in\n");


    struct capref josh_in;
    aos_rpc_call(josh_rpc, DISP_IFACE_GET_STDIN, &josh_in);

    debug_printf("got josh in\n");
    //aos_rpc_call(lpuart_rpc, DISP_IFACE_SET_STDOUT, josh_in);*/


    /*for (int i = 0; i < 20; i++) {
        spawn_new_domain("client",NULL);
        for (int j = 0; j < 10; j++) {
            err = event_dispatch(get_default_waitset());
        }
    }*/

  

    // spawn_new_domain("nameservicetest",0,NULL,NULL,NULL_CAP,NULL_CAP,NULL);

    spawn_new_core(my_core_id + 1);
    spawn_new_domain("server a",0,NULL,NULL,NULL_CAP,NULL_CAP,NULL);
    // spawn_new_domain("server b",1,NULL,NULL,NULL_CAP,NULL_CAP,NULL);
    //spawn_new_domain("server a",0,NULL,NULL,NULL_CAP,NULL_CAP,NULL);
    //spawn_new_domain("server b",0,NULL,NULL,NULL_CAP,NULL_CAP,NULL);
    // spawn_new_domain("server c",0,NULL,NULL,NULL_CAP,NULL_CAP,NULL);
    // spawn_new_domain("server d",0,NULL,NULL,NULL_CAP,NULL_CAP,NULL);
    // spawn_new_core(my_core_id + 2);
    // spawn_new_core(my_core_id + 3);


    // 
    
    //run_init_tests(my_core_id);

    
    
    //run_init_tests(my_core_id);


    

    // Grading
    grading_test_early();

    // TODO: Spawn system processes, boot second core etc. here
    





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
    
    // run_init_tests(my_core_id);

    //spawn_new_domain("client", NULL, NULL_CAP, NULL);

    // spawn_new_domain("memeater",NULL,NULL_CAP);
    // spawn_new_domain("server", NULL, NULL_CAP);

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




