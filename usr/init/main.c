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
    test_spawn_load_argv();

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
