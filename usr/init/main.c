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
#include <aos/deferred.h>

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
#include <hashtable/hashtable.h>
#include <aos/fs_service.h>


#include "routing.h"
#include <aos/kernel_cap_invocations.h>

#include <process_manager_interface.h>
#include <fs/fs.h>




// #include <aos/curdispatcher_arch.h>


coreid_t my_core_id;


static errval_t init_foreign_core(void){
    errval_t err;
    set_pm_online();

    uint64_t *urpc_init = (uint64_t*) MON_URPC_VBASE;
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
    
    err = frame_forge(cap_mmstrings, urpc_init[4], urpc_init[5], disp_get_current_core_id());
    ON_ERR_RETURN(err);
    
    for(int i = 0; i < bi -> regions_length;++i) {
        
        if(bi -> regions[i].mr_type == RegionType_Module){
            struct capref module_cap = {
                .cnode = cnode_module,
                .slot = bi -> regions[i].mrmod_slot
            };
            size_t size = bi->regions[i].mrmod_size;
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

    routing_ht = create_hashtable();
    return SYS_ERR_OK;
}

__unused
static errval_t init_filesystemserver(void)
{
    errval_t err;
    struct spawninfo *fs_si;
    err = spawn_filesystem("filesystemserver", &fs_si);
    return err;
}



static errval_t init_name_server(void){
    errval_t err;

    routing_ht = create_hashtable();

    struct spawninfo *ns_si = spawn_create_spawninfo();
    domainid_t *ns_pid = &ns_si -> pid;
    err = spawn_load_by_name("nameserver",ns_si,ns_pid);
    *ns_pid = 0;
    ON_ERR_RETURN(err);


    struct aos_rpc *rpc  = &ns_si->rpc;

    aos_rpc_set_interface(rpc, get_init_interface(), INIT_IFACE_N_FUNCTIONS, malloc(INIT_IFACE_N_FUNCTIONS * sizeof(void *)));
    initialize_rpc_handlers(rpc);

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
static errval_t start_josh_on_serial(void)
{
    errval_t err;

    // use ump communication as it is simpler to use
    struct capref term_in;
    struct capref term_out;
    frame_alloc(&term_in, BASE_PAGE_SIZE, NULL);
    frame_alloc(&term_out, BASE_PAGE_SIZE, NULL);

    struct spawninfo *term_si;
    spawn_lpuart_driver("lpuart_terminal", &term_si, term_in, term_out);
    
    struct spawninfo *josh_si;
    err = spawn_new_domain("josh", 0, NULL, NULL, NULL_CAP, term_in, term_out, &josh_si);
    return err;
}


__unused static void handle_fast_RTT(void * arg){
    debug_printf("Here\n");
    return;
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
    spawn_new_core(2);
    spawn_new_core(3);
    barrelfish_usleep(500000);

    debug_printf(">> START filesystem server\n");
    init_filesystemserver();

    while(!get_fs_online()){
        err = event_dispatch(get_default_waitset());
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed waitset");
        }
    }

    struct spawninfo *enet_si;
    debug_printf("lets go\n");
    spawn_enet_driver("enet", &enet_si);

    start_josh_on_serial();




    // struct periodic_event pe;
 
    // err = periodic_event_create(&pe,get_default_waitset(),1000000,MKCLOSURE(print_hello,NULL));
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"Failed to create even closure\n");
    // }
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

  

    // spawn_new_domain("client_perf",0,NULL,NULL,NULL_CAP,NULL_CAP,NULL_CAP,NULL);

    // spawn_new_core(my_core_id + 1);



    // 
    
    //run_init_tests(my_core_id);

    
    
    //run_init_tests(my_core_id);


    //for (volatile size_t i = 0; i < 1000000000; i++);

    // debug_printf(">> INIT filesystem server\n");
    // for (volatile size_t i = 0; i < 100; i++){
    //     thread_yield();
    // }



    //spawn_new_domain("hello", 0, NULL, NULL, NULL_CAP, NULL_CAP, NULL_CAP, NULL);

    // Grading
    grading_test_early();

    // TODO: Spawn system processes, boot second core etc. here
    





    // Grading
    grading_test_late();

    // debug_printf("Message handler loop\n");

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

    err = init_foreign_core();
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to initialize ram and bootinfo for new core core\n");
    }
    

    grading_setup_app_init(bi);

    grading_test_early();

    grading_test_late();
    


  

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

    // debug_printf("init: on core %" PRIuCOREID ", invoked as:", my_core_id);
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




