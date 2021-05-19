/**
 * \file
 * \brief Hello world application
 */

#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>

#include <aos/aos_rpc.h>
/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */
__attribute__((unused))
    static void stack_overflow(void){
    char c[1024];
    c[1] = 1;
    debug_printf("Stack address: %lx\n",c);
    stack_overflow();
}

#include <stdio.h>

#include <aos/aos.h>

int main(int argc, char *argv[])
{
    
    printf("Hello, world! from userspace\n");


    for(int i = 0; i < argc; ++i){
        printf("Argv[%d] = %s\n",i,argv[i]);
    }



    domainid_t did;
    // debug_printf("spawning test binary '%s'\n", hello);
    aos_rpc_process_spawn(get_init_rpc(), "hello  a", disp_get_core_id(), &did);

    // domainid_t did;
    // aos_rpc_call(get_init_rpc(),INIT_IFACE_SPAWN,"hello a",disp_get_core_id(),&did);
    // printf("%s\n", argv[1]);
    // stack_overflow();

    // errval_t err;
    // char * name;
    

    // debug_printf("Trying to resolve name\n");
    // struct aos_rpc * init_rpc = get_init_rpc();
    // debug_printf("Got initrpc!\n");



    return EXIT_SUCCESS;
}
