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
#include "mem_alloc.h"




struct bootinfo *bi;

coreid_t my_core_id;


__attribute__((unused)) static void test_free_coalesce(void)
{
    const size_t n = 64;
    struct capref caps[n];
    for (int i = 0; i < n; i++) {
        ram_alloc_aligned(&caps[i], 4096, 1);
    }
    for (int i = 0; i < n; i++) {
        if ((i / 10) % 2) {
            aos_ram_free(caps[i]);
        }
    }
    print_mm_state(&aos_mm);

    for (int i = 0; i < n; i++) {
        if ((i / 10) % 2) {
            ram_alloc_aligned(&caps[i], 4096, 1);
        }
    }

    print_mm_state(&aos_mm);
}


__attribute__((unused)) static void many_allocs_and_frees(void)
{
    const size_t n = 100000;
    for (int i = 0; i < n; i++) {
        struct capref cap;
        ram_alloc_aligned(&cap, 4096, 1);
        aos_ram_free(cap);
    }

    print_mm_state(&aos_mm);
}


__attribute__((unused)) static long sum_until(long m)
{
    long result = 0;
    for (long i = 0; i < m; i++) result += i;
    return result;
}

__attribute__((unused)) static void test_map_big(lvaddr_t base, size_t size)
{
    printf("mapping big: 0x%lx\n", size);
    struct capref my_frame;
    size_t f_size;
    frame_alloc(&my_frame, size, &f_size);

    lvaddr_t addr = base;
    paging_map_fixed_attr(get_current_paging_state(), addr, my_frame, size, VREGION_FLAGS_READ_WRITE);

    printf("mapped big: 0x%lx\n", size);
    long* pointer = (long*) addr;
    for (int i = 0; i < size / sizeof(long); i++) {
        pointer[i] = i;
    }
    for (int i = 0; i < size / sizeof(long); i++) {
        pointer[0] += pointer[i];
    }
    assert(pointer[0] == sum_until(size / sizeof(long)));
    printf("mapped and accessed 0x%x bytes of memory\n", size);
    printf("value in memory at v-address %p: %d\n", pointer, pointer[0]);
}


__attribute__((unused)) static void test_big_mappings(void)
{
    lvaddr_t base = VADDR_OFFSET + 0x10000000UL;
    for (int i = 1; i < 120; i++) {
        test_map_big(base, i * BASE_PAGE_SIZE);
        base += i * BASE_PAGE_SIZE;
    }
}


__attribute__((unused)) static void test_align(void)
{
    struct capref cap;
    ram_alloc_aligned(&cap, 4096, 1024 * 1024 * 1024);
    print_mm_state(&aos_mm);
}




__attribute__((unused)) static void test_spawn_load_argv(void){
    //=============SPAWN PROCESS HELLO==================//
    errval_t err;
    printf("Trying to spawn process hello\n" );


    //err = spawn_load_by_name(1,p_argv,&si,&pid);
    for (int i = 0; i < 20; i++) {
        struct spawninfo *si = malloc(sizeof(struct spawninfo));
        domainid_t *pid = malloc(sizeof(domainid_t));
        err = spawn_load_by_name("hello", si, pid);
        if(err_is_fail(err)){
            DEBUG_ERR(err, "spawn loading failed");
        }
        free(si);
        free(pid);
    }
    print_mm_state(&aos_mm);

    struct spawninfo si1;
    domainid_t pid1;
    err = spawn_load_by_name("hello", &si1, &pid1);
    if(err_is_fail(err)){
        DEBUG_ERR(err, "spawn loading failed");
    }
    // err = spawn_load_by_name()

    //===========================================//

}




__attribute__((unused)) static void test(void)
{
    // begin experiment
    printf("start experiment!\n");
    //test_align();
    //many_allocs_and_frees();
    // //test_free_coalesce();
    // test_big_mappings();
    //
    // struct capref my_frame;
    // size_t f_size;
    // frame_alloc(&my_frame, 4096, &f_size);
    //
    // lvaddr_t addr = VADDR_OFFSET + 0x123000;
    // paging_map_fixed_attr(get_current_paging_state(), addr, my_frame, 4096, VREGION_FLAGS_READ_WRITE);
    //
    // int* pointer = (int*) addr;
    // for (int i = 0; i < 100; i++) {
    //     pointer[i] = i;
    // }
    // err = lmp_chan_send2(&rpc -> channel, LMP_SEND_FLAGS_DEFAULT,NULL_CAP,NUMBER,num);
    // if(err_is_fail(err)){
    //   DEBUG_ERR(err,"failed to send number");
    // }
    // for (int i = 0; i < 100; i++) {
    //     pointer[0] += pointer[i];
    // }
    // printf("value in memory at v-address %p: %d\n", pointer, pointer[0]);
    /*ram_alloc_aligned(&a_page, 4096, 4096);
    ram_alloc_aligned(&a_page, 4096, 4096);
    ram_alloc_aligned(&a_page, 4096, 4096);
    ram_alloc_aligned(&a_page, 4096, 4096);*/

    printf("end experiment!\n");
    // end experiment
}


__attribute__((unused)) static void faulty_allocations(void)
{
    struct capref cap;
    ram_alloc_aligned(&cap, 4096, 1024 * 1024 * 1024);
    print_mm_state(&aos_mm);
}


__attribute__((unused)) static void init_handler(void *arg)
{
    debug_printf("init_handler called\n");
    struct lmp_chan *channel = arg;
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref cap;
    cap = NULL_CAP;
    errval_t err = lmp_chan_recv(channel, &msg, &cap);
    if(err_is_fail(err)){
      DEBUG_ERR(err,"Event called, but receive failed");
    }
    //start switch
    switch(msg.words[0]){
      case AOS_RPC_INIT:
      //CASE INIT
        if (!capref_is_null(cap)) {
            debug_printf("received capability\n");
            char buuuf[256];
            debug_print_cap_at_capref(buuuf, sizeof buuuf, cap);
            debug_printf("received cap is: %s\n", buuuf);
            channel->remote_cap = cap;

            err = lmp_chan_send1(channel,LMP_SEND_FLAGS_DEFAULT,NULL_CAP, AOS_RPC_ACK);
            if(err_is_fail(err)){
              DEBUG_ERR(err,"Could not send ack for init");
            }
        }
        else {
            debug_printf("no cap received");
        }

        lmp_chan_register_recv(channel, get_default_waitset(), MKCLOSURE(&init_handler, arg));
        //END case INIT


    }

    //end switch
    // if(err_is_fail(err) && lmp_err_is_transient(err)) {
    //     lmp_chan_register_recv(channel, get_default_waitset(), MKCLOSURE(&init_handler, arg));
    //     return;
    // }
    //
    // if (!capref_is_null(cap)) {
    //     debug_printf("received capability\n");
    //     char buuuf[256];
    //     debug_print_cap_at_capref(buuuf, sizeof buuuf, cap);
    //     debug_printf("received cap is: %s\n", buuuf);
    //     channel->remote_cap = cap;
    // }
    // else {
    //     debug_printf("no cap received");
    // }
    //
    // debug_printf("Recieved %d words!\n", msg.buf.msglen);
    // for (int i = 0; i < msg.buf.msglen; i++) {
    //     debug_printf("%d: %ld\n", i, msg.words[i]);
    // }
    // lmp_chan_register_recv(channel, get_default_waitset(), MKCLOSURE(&init_handler, arg));
}

__attribute__((unused)) static void spawn_memeater(void)
{
    struct spawninfo *si1 = malloc(sizeof(struct spawninfo));
    domainid_t *pid1 = malloc(sizeof(domainid_t));
    errval_t err = spawn_load_by_name("memeater", si1, pid1);
    if(err_is_fail(err)){
        DEBUG_ERR(err, "spawn loading failed");
    }

    //err = lmp_chan_accept(&si1->channel, DEFAULT_LMP_BUF_WORDS, NULL_CAP);
    //DEBUG_ERR(err, "accepting");
    //
    //bool can_receive = lmp_chan_can_recv(&si1->channel);
    //printf("Trying to receive: %d\n", can_receive);

    //slot_alloc(&si1->channel.endpoint->recv_slot);
    //struct cnoderef cnode;
    //cnode_create_l2(&si1->channel.endpoint->recv_slot, &cnode);
    //si1->channel.endpoint->k.recv_cspc = get_cap_addr();

    err = lmp_chan_alloc_recv_slot(&si1->channel);
    DEBUG_ERR(err, "alloc recv slot");

    err = lmp_chan_register_recv(&si1->channel, get_default_waitset(), MKCLOSURE(&init_handler, &si1->channel));
    DEBUG_ERR(err, "register recv");
    /*while(!can_receive){
      can_receive = lmp_chan_can_recv(&si1 -> channel);
    }


    printf("Can receive: %d\n", can_receive);
    struct lmp_recv_msg msg;
    msg.buf.buflen = LMP_MSG_LENGTH;
    struct capref rec_cap;
    printf("Trying to receive a message:\n");
    err = lmp_chan_recv(&si1 -> channel, &msg, &rec_cap);
    uint32_t rec_word = *msg.words;
    printf("Received message: %d\n",rec_word );*/
}




static int
bsp_main(int argc, char *argv[]) {
    errval_t err;

    // Grading
    grading_setup_bsp_init(argc, argv);

    // First argument contains the bootinfo location, if it's not set
    bi = (struct bootinfo*)strtol(argv[1], NULL, 10);
    assert(bi);

    err = initialize_ram_alloc();
    if(err_is_fail(err)){
        DEBUG_ERR(err, "initialize_ram_alloc");
    }

    // TODO: initialize mem allocator, vspace management here

    //test();
    spawn_memeater();

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

static int
app_main(int argc, char *argv[]) {
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



    if(my_core_id == 0) return bsp_main(argc, argv);
    else                return app_main(argc, argv);
}
