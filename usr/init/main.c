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


#include <spawn/spawn.h>
#include <spawn/process_manager.h>
#include "mem_alloc.h"

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

static void handler_putchar(struct aos_rpc *r, uintptr_t c) {
    putchar(c);
    //debug_printf("recieved: %c\n", (char)c);
}

static void handler_getchar(struct aos_rpc *r, uintptr_t *c) {
    int v = getchar();
    //debug_printf("getchar: %c\n", v);
    *c = v;//getchar();
}

static void req_ram(struct aos_rpc *r, uintptr_t size, uintptr_t alignment, struct capref *cap, uintptr_t *ret_size) {
    ram_alloc_aligned(cap, size, alignment);
}

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

static void recv_number(struct aos_rpc *r, uintptr_t number) {
    debug_printf("recieved number: %ld\n", number);
}

static void recv_string(struct aos_rpc *r, const char *string) {
    debug_printf("recieved string: %s\n", string);
}

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


static int bsp_main(int argc, char *argv[])
{
    errval_t err;

    // Grading
    grading_setup_bsp_init(argc, argv);

    // First argument contains the bootinfo location, if it's not set
    bi = (struct bootinfo *)strtol(argv[1], NULL, 10);
    assert(bi);



    err = initialize_ram_alloc();
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "initialize_ram_alloc");
    }

    struct terminal_state ts;
    ts.reading = false;
    ts.index = 0;
    ts.waiting = NULL;
    terminal_state = &ts;
    // TODO: initialize mem allocator, vspace management here

    // test();
    // debug_printf("input:\n");

    // char b = getchar();
    // debug_printf("Character from getchar = %c\n",b);
    // char a = getchar();
    //
    // debug_printf("Character from getchar = %c\n",a);


    // spawn_memeater();
    // printf("Hello!\n");

    lvaddr_t addr = 16000000000000;
    char *test = (char * ) addr;
    char c = test[0];
    printf("%c\n",c);

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


    // printf("requesting char\n");
    // char c = 'A';
    // char buff[10];
    // int i = 0;
    // while(c != 13 && i < 9){
    //   debug_printf("Loop iteration: %d\n",i);
    //   c = getchar();
    //   buff[i] = c;
    //   i++;
    // }
    // buff[i] = '\0';
    //
    // printf("Received string :%s\n",buff);

    if (my_core_id == 0)
        return bsp_main(argc, argv);
    else
        return app_main(argc, argv);
}
