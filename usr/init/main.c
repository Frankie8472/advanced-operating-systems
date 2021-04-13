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

__attribute__((unused))
static void stack_overflow(void){
  char c[10];
  debug_printf("Stack address: %lx\n",c);
  stack_overflow();
}

__attribute__((unused))
static void infinite_loop(void){

  uint64_t count = 0;
  while(true){
    size_t size = 1L << 29;
    char* p = malloc(size * sizeof(char));
    for(int i = 0; i < size;++i){
        p[i] = i % 255;
    }
    uint64_t random[10] = { 1414141,54565791,1894418,235,9265};
    for(int i = 0; i < 10; ++i){
        assert(p[random[i]] == random[i]%255);
    }
    free(p);
    if(count % 1000 == 0){
        debug_printf("Ran %ld times\n",count);
        break;
    }  
    count++;
  }
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

 

    // debug_print_paging_state(*ps);
    // lvaddr_t addr = ps -> current_address;
    // debug_printf("%d trying to access address\n",addr);
    // char *test = (char * ) addr;
    // char c = test[0];
    // printf("%c\n",c);

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

       struct paging_state* ps = get_current_paging_state();
    debug_print_paging_state(*ps);



    // struct morecore_state* ms =  get_morecore_state();
    // debug_printf("Current header_base: %lx\n", ms -> header_base);
    // debug_printf("Current header_freep: %lx\n", ms -> header_freep);
    // debug_printf("Paging region:\n");
    // debug_printf("Staic morecore freep %lx:\n",ms -> freep);
    // debug_print_paging_region(ms -> region);
    
    // char c[17300000];
    // debug_printf("Address: %ld\n",c);
    // // char d[1024];
    // debug_printf("Address: %ld\n",c + sizeof(c));
    // {
    //     char c[1730000];
    //     debug_printf("Address: %ld\n",c);
    // }
    // lvaddr_t  addr = NULL;
    // char *test = NULL;
    // char c = test[0];
    // printf("%c\n",c);

    double* p = (double *) malloc(10000000 * sizeof(double));
    // p = p;
    printf("%lx\n",&p); 
    debug_printf("Malloced address is at:%lx\n",p);
    p[0] = 1;
    p[171718414] = 1;
    debug_printf("p[0] = %f\n",p[0]);
    debug_printf("p[171718414]=%f\n",p[171718414]);




    double* p2 = (double *) malloc((100000000) * sizeof(double));
    debug_printf("Malloced address is at:%lx\n",p2);
    p2[10839000] = 3;
    debug_printf("p[10839000] = %f\n",p2[10839000]);


    debug_print_paging_state(*ps);


    infinite_loop();
    // p[1289411] = 1;
    
    // debug_print_paging_state(*ps);

    // double* p2 = (double *) malloc(1024 * sizeof(double));
    // debug_printf("Malloced address is at:%lx\n",p2);


    // debug_print_paging_state(*ps);
    // debug_printf("Malloced at  to address: %lx\n",p,&p[1024 - 1]); 
    // stack_overflow();
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
