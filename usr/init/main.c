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
#include "mem_alloc.h"
#include <aos/coreboot.h>
#include "test.h"

#include <aos/cache.h>
#include <aos/kernel_cap_invocations.h>


// #include <aos/curdispatcher_arch.h>


struct terminal_queue {
    struct lmp_chan* cur;
    struct terminal_queue* next;
};

struct terminal_state{
  bool reading;
  char to_put[1024];
  size_t index;
  struct terminal_queue* waiting;
};

struct terminal_state *terminal_state;
struct bootinfo *bi;


struct aos_rpc* core_channels[4];



coreid_t my_core_id;

errval_t initialize_rpc(struct spawninfo *si);

/**
 * \brief handler function for putchar rpc call
 */
static void handler_putchar(struct aos_rpc *r, uintptr_t c) {
    putchar(c);
    //debug_printf("recieved: %c\n", (char)c);
}

/**
 * \brief handler function for getchar rpc call
 */
static void handler_getchar(struct aos_rpc *r, uintptr_t *c) {
    int v = getchar();
    //debug_printf("getchar: %c\n", v);
    *c = v;//getchar();
}

/**
 * \brief handler function for ram alloc rpc call
 */
static void req_ram(struct aos_rpc *r, uintptr_t size, uintptr_t alignment, struct capref *cap, uintptr_t *ret_size) {
    errval_t err = ram_alloc_aligned(cap, size, alignment);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "Error in remote ram allocation!\n");
    }
}

/**
 * \brief handler function for initiate rpc call
 * 
 * This function is called by a domain who wishes to make use of the
 * rpc interface of the init domain
 */
static void initiate(struct aos_rpc *rpc, struct capref cap) {
    debug_printf("rpc: %p\n", rpc);
    rpc->channel.lmp.remote_cap = cap;
}

// static void  foreign_req_ram(struct aos_rpc *old_rpc,uintptr_t size){
//     err = ram_alloc_aligned()
// }


static void spawn_handler(struct aos_rpc *old_rpc, const char *name, uintptr_t core_id, uintptr_t *new_pid) {


    if(core_id == disp_get_core_id()){
        struct spawninfo *si = spawn_create_spawninfo();

        domainid_t *pid = &si->pid;
        spawn_load_by_name((char*) name, si, pid);
        *new_pid = *pid;
        struct aos_rpc *rpc = &si->rpc;
        aos_rpc_init(rpc, si->cap_ep, NULL_CAP, si->lmp_ep);
        initialize_rpc(si);
        errval_t err = lmp_chan_register_recv(&rpc->channel.lmp, get_default_waitset(), MKCLOSURE(&aos_rpc_on_message, &rpc));
        if (err_is_fail(err) && err == LIB_ERR_CHAN_ALREADY_REGISTERED) {
            // not too bad, already registered
        }
    }else{
        errval_t err;
        struct aos_rpc* ump_chan = core_channels[core_id];
        err = aos_rpc_call(ump_chan,AOS_RPC_FOREIGN_SPAWN,name,new_pid);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to call aos rpc in spawn handler for foreign core\n");
        }
        
    }


}


/**
 * \brief handler function for send number rpc call
 */
static void recv_number(struct aos_rpc *r, uintptr_t number) {
    debug_printf("recieved number: %ld\n", number);
}

/**
 * \brief handler function for send string rpc call
 */
static void recv_string(struct aos_rpc *r, const char *string) {
    debug_printf("recieved string: %s\n", string);
}

/**
 * \brief initialize all handlers for rpc calls
 * 
 * Init needs to provide a bunch of rpc interfaces to other processes.
 * This function is calld each time a new process is spawned and makes sure
 * that all necessary rpc handlers in si->rpc are set. It then registers
 * the lmp recieve handler.
 */
errval_t initialize_rpc(struct spawninfo *si)
{
    struct aos_rpc *rpc = &si->rpc;
    aos_rpc_register_handler(rpc, AOS_RPC_INITIATE, &initiate);
    aos_rpc_register_handler(rpc, AOS_RPC_SEND_NUMBER, &recv_number);
    aos_rpc_register_handler(rpc, AOS_RPC_SEND_STRING, &recv_string);

    aos_rpc_register_handler(rpc, AOS_RPC_REQUEST_RAM, &req_ram);

    aos_rpc_register_handler(rpc, AOS_RPC_PROC_SPAWN_REQUEST, &spawn_handler);

    aos_rpc_register_handler(rpc, AOS_RPC_PUTCHAR, &handler_putchar);
    aos_rpc_register_handler(rpc, AOS_RPC_GETCHAR, &handler_getchar);

    void instant_return(struct aos_rpc *r) { return; }
    aos_rpc_register_handler(rpc, AOS_RPC_ROUNDTRIP, &instant_return);

    errval_t err = lmp_chan_register_recv(&rpc->channel.lmp, get_default_waitset(), MKCLOSURE(&aos_rpc_on_message, &rpc));
    return err;
}


__attribute__((unused)) static void spawn_memeater(void)
{
    struct spawninfo *memeater_si = spawn_create_spawninfo();

    domainid_t *memeater_pid = &memeater_si->pid;
    errval_t err = spawn_load_by_name("memeater", memeater_si, memeater_pid);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "spawn loading failed");
    }

    struct aos_rpc *rpc = &memeater_si->rpc;
    aos_rpc_init(rpc, memeater_si->cap_ep, NULL_CAP, memeater_si->lmp_ep);

    err = lmp_chan_alloc_recv_slot(&rpc->channel.lmp);

    err = initialize_rpc(memeater_si);
}



__attribute__((unused)) static void spawn_page(void){
    errval_t err;
    debug_printf("Spawning hello\n");
    struct spawninfo *hello_si = spawn_create_spawninfo();
    domainid_t *hello_pid = &hello_si->pid;
    err = spawn_load_by_name("hello", hello_si, hello_pid);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "spawn loading failed");
    }

    struct aos_rpc *rpc = &hello_si->rpc;
    aos_rpc_init(rpc, hello_si->cap_ep, NULL_CAP, hello_si->lmp_ep);
    err = lmp_chan_alloc_recv_slot(&rpc->channel.lmp);
    ON_ERR_NO_RETURN(err);

    err = initialize_rpc(hello_si);
    ON_ERR_NO_RETURN(err);
}

int real_main(int argc, char *argv[]);
int real_main(int argc, char *argv[])
{

    debug_printf(">> Entering Real Main\n");

    errval_t err;

    struct terminal_state ts;
    ts.reading = false;
    ts.index = 0;
    ts.waiting = NULL;
    terminal_state = &ts;

    struct paging_state* ps = get_current_paging_state();
    debug_print_paging_state(*ps);

    //run_init_tests(0);

    
    /*void stack_overflow(void) {
        char c[1024];
        c[1] = 1;
        debug_printf("Stack address: %lx\n",c);
        stack_overflow();
    }
    stack_overflow();*/
    //spawn_page();
  

    // TODO: initialize mem allocator, vspace management here

    spawn_memeater();

    // benchmark_mm();


    // Grading
    grading_test_early();
#pragma GCC diagnostic ignored "-Wunused-variable"
    // TODO: Spawn system processes, boot second core etc. here
    const char * boot_driver = "boot_armv8_generic";
    const char * cpu_driver = "cpu_imx8x";
    const char * init = "init";
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
    debug_printf("Here is bi in bsp: %lx \n",bi);
    debug_printf("Bootinfo base: %lx, bootinfo size: %lx\n",get_phys_addr(bootinfo_cap),BOOTINFO_SIZE);
    debug_printf("Core ram base: %lx, core ram size: %lx\n",get_phys_addr(core_ram),get_phys_size(core_ram));
    urpc_init[0] = get_phys_addr(bootinfo_cap);
    urpc_init[1] = BOOTINFO_SIZE;
    urpc_init[2] = get_phys_addr(core_ram);
    urpc_init[3] = get_phys_size(core_ram);
    urpc_init[4] = get_phys_addr(cap_mmstrings);
    urpc_init[5] = get_phys_size(cap_mmstrings);

    cpu_dcache_wbinv_range((vm_offset_t) urpc_data, BASE_PAGE_SIZE);

    coreid_t coreid = 1;
    err = coreboot(coreid,boot_driver,cpu_driver,init,urpc_frame_id);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to boot core");
    }
    
    
    
    struct aos_rpc *ump_rpc_test = malloc(sizeof(struct aos_rpc));
    aos_rpc_init_ump(ump_rpc_test, (lvaddr_t) urpc_init, BASE_PAGE_SIZE, true);
    
    core_channels[coreid] = ump_rpc_test;

    aos_rpc_register_handler(ump_rpc_test, AOS_RPC_SEND_NUMBER, &recv_number);
    
    //domainid_t pid;
    //err = aos_rpc_call(ump_rpc_test, AOS_RPC_FOREIGN_SPAWN, "memeater", 1, &pid);

    /*int poller(void *arg) {
        struct ump_poller *p = arg;
        ump_chan_run_poller(p);
        return 0;
    }

    struct ump_poller *init_poller = ump_chan_get_default_poller();

    struct thread *pollthread = thread_create(&poller, init_poller);
    pollthread = pollthread;*/
    //================================================



    // err = aos_rpc_process_spawn(get_init_rpc(),)
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"Failed to spawn process in different core\n");
    // }



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

    thread_exit(0);
    return EXIT_SUCCESS;
}

void switch_stack(void *function, void *new_stack, uintptr_t arg0, uintptr_t arg1);
void switch_stack(void *function, void *new_stack, uintptr_t arg0, uintptr_t arg1)
{
    __asm volatile (
        "mov x29, sp\n"
        "mov sp, %[stack]\n"
        "mov x0, %[argc]\n"
        "mov x1, %[argv]\n"
        "blr %[func]\n"
        "mov sp, x29\n"
        :
        :
            [func]  "r"(function),
            [stack] "r"(new_stack),
            [argc]  "r"(arg0),
            [argv]  "r"(arg1)
        :
            "x0", "x1", "x29"
    );
}


static int bsp_main(int argc, char *argv[])
{
    errval_t err;

    // Grading
    grading_setup_bsp_init(argc, argv);

    // First argument contains the bootinfo location, if it's not set
    bi = (struct bootinfo *) strtol(argv[1], NULL, 10);
    assert(bi);


    

    err = initialize_ram_alloc();
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "/>/> Error: Initialize_ram_alloc");
    }




    struct paging_state *st = get_current_paging_state();
    struct paging_region hacc_stacc_region;
    uint64_t stacksize = 1L << 20;    
    err = paging_region_init(st, &hacc_stacc_region, stacksize, VREGION_FLAGS_READ_WRITE);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "/>/> Error: Creating stack region\n");
    }

    hacc_stacc_region.type = PAGING_REGION_STACK;
    snprintf(hacc_stacc_region.region_name, sizeof(hacc_stacc_region.region_name), "hacc stacc %d", 0);
    err = paging_map_stack_guard(st, hacc_stacc_region.base_addr);

    lvaddr_t top = hacc_stacc_region.base_addr + stacksize - 1;
    top = ROUND_DOWN(top, 32);

    // Call real_main with a new stack
    /*debug_printf(">> Switching to new stack at %p with size of %ld B\n", (void *) top, stacksize);
    switch_stack(&real_main, (void*) top, argc, (uintptr_t) argv);
    debug_printf(">> Returning to old stack\n");*/
    
    real_main(argc, argv);

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
    void spawny(struct aos_rpc *origin_rpc, const char *name, uintptr_t core_id, uintptr_t *new_pid)
    {
        debug_printf("WE SPAWN: %s, %ld\n", name, core_id);
        struct spawninfo *si = spawn_create_spawninfo();

        domainid_t *pid = &si->pid;
        spawn_load_by_name((char*) name, si, pid);
        *new_pid = *pid;
        struct aos_rpc *rpc = &si->rpc;
        aos_rpc_init(rpc, si->cap_ep, NULL_CAP, si->lmp_ep);
        initialize_rpc(si);
        errval_t errr = lmp_chan_register_recv(&rpc->channel.lmp, get_default_waitset(), MKCLOSURE(&aos_rpc_on_message, &rpc));
        if (err_is_fail(errr) && errr == LIB_ERR_CHAN_ALREADY_REGISTERED) {
            // not too bad, already registered
        }
    }
    
    struct aos_rpc *ump_rpc_test = malloc(sizeof(struct aos_rpc));
    aos_rpc_init_ump(ump_rpc_test, (lvaddr_t) urpc_init, BASE_PAGE_SIZE, false);
    aos_rpc_register_handler(ump_rpc_test, AOS_RPC_FOREIGN_SPAWN, spawny);
    core_channels[0] = ump_rpc_test;

    int poller(void *arg) {
        struct ump_poller *p = arg;
        ump_chan_run_poller(p);
        return 0;
    }

    struct ump_poller *init_poller = ump_chan_get_default_poller();

    struct thread *pollthread = thread_create(&poller, init_poller);
    pollthread = pollthread;

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
    




    // bi = (struct bootinfo *) strtol(argv[1], NULL, 10);

    
    

    // printf()

    // for(int i = 0; i < argc;++i){
    //     printf("%s | ",argv[i]);
    // }
    // debug_printf("\n");



 


    // struct aos_rpc *rpc = &hello_si->rpc;
    // aos_rpc_init(rpc, hello_si->cap_ep, NULL_CAP, hello_si->lmp_ep);
    // err = lmp_chan_alloc_recv_slot(&rpc->channel);
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"AFiled to setup channel\n");
    // }
    // err = initialize_rpc(hello_si);
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"AFiled to setup channel\n");
    // }

    debug_printf("Hello from core: %d!\n",disp_get_current_core_id());
    err = init_foreign_core();
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to initialize ram and bootinfo for new core core\n");
    }


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
