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
    ram_alloc_aligned(cap, size, alignment);
}

/**
 * \brief handler function for initiate rpc call
 * 
 * This function is called by a domain who wishes to make use of the
 * rpc interface of the init domain
 */
static void initiate(struct aos_rpc *rpc, struct capref cap) {
    debug_printf("rpc: %p\n", rpc);
    rpc->channel.remote_cap = cap;
}

static void spawn_handler(struct aos_rpc *old_rpc, const char *name, uintptr_t core_id, uintptr_t *new_pid) {
    struct spawninfo *si = spawn_create_spawninfo();

    domainid_t *pid = &si->pid;
    spawn_load_by_name((char*) name, si, pid);
    *new_pid = *pid;

    struct aos_rpc *rpc = &si->rpc;
    aos_rpc_init(rpc, si->cap_ep, NULL_CAP, si->lmp_ep);
    initialize_rpc(si);

    errval_t err = lmp_chan_register_recv(&rpc->channel, get_default_waitset(), MKCLOSURE(&aos_rpc_on_message, &rpc));
    if (err_is_fail(err) && err == LIB_ERR_CHAN_ALREADY_REGISTERED) {
        // not too bad, already registered
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

    errval_t err = lmp_chan_register_recv(&rpc->channel, get_default_waitset(), MKCLOSURE(&aos_rpc_on_message, &rpc));
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

    err = lmp_chan_alloc_recv_slot(&rpc->channel);

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
    err = lmp_chan_alloc_recv_slot(&rpc->channel);
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

    err = coreboot(1,boot_driver,cpu_driver,init,urpc_frame_id);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to boot core");
    }

    char *urpc_data = NULL;
    paging_map_frame_complete(get_current_paging_state(), (void **) &urpc_data, urpc_cap, NULL, NULL);
    for (int i = 0; i < 10; i++) {
        debug_printf("reading urpc frame: %s\n", urpc_data);
        for (int j = 0; j < 1000 * 1000 * 10; j++) {
            __asm volatile("mov x0, x0");
        }
    }

    //boot_core

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
    size_t stacksize = 1 << 20;     // 1MB (set to 1GB if overflow check works)
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

static int app_main(int argc, char *argv[])
{
    // Implement me in Milestone 5
    // Remember to call
    // - grading_setup_app_init(..);
    // - grading_test_early();
    // - grading_test_late();
    
    
    

    bi = (struct bootinfo *) strtol(argv[1], NULL, 10);
    grading_setup_app_init(bi);

    for (int i = 0; i < argc; i++) {
        debug_printf("argv[%d]: %s\n", i, argv[i]);
    }

    debug_printf("Hello from second core!\n");
    struct capref urpc_frame = {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_MON_URPC
    };

    struct capability urpc_cap;

    invoke_cap_identify(urpc_frame, &urpc_cap);

    debug_printf("urpc_frame at: %p\n", get_address(&urpc_cap));
    debug_printf("urpc_frame size: %p\n", get_size(&urpc_cap));
    char *urpc_data = (char *) MON_URPC_VBASE;


    strcpy(urpc_data, "Hello World!\n");
    
    grading_test_early();

    grading_test_late();
    return LIB_ERR_NOT_IMPLEMENTED;
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
