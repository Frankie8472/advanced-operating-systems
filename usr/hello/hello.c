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
    
    // printf("Hello, world! from userspace\n");
    // printf("%s\n", argv[1]);
    // stack_overflow();

    errval_t err;
    // char * name;
    // char buffer[512];
    debug_printf("Trying to resolve name\n");
    struct aos_rpc * init_rpc = get_init_rpc();
    debug_printf("Got initrpc!\n");

    coreid_t cid;
    err = aos_rpc_call(init_rpc, AOS_RPC_GET_PROC_CORE,3,&cid);

    debug_printf("Got core id : %d\n",cid);



    // aos_rpc_call(init_rpc, AOS_INIT_NEW_CHANNEL )


    // err = aos_rpc_call(init_rpc,AOS_RPC_GET_PROC_NAME,0,buffer);
    // // err = aos_rpc_process_get_name(aos_rpc_get_process_channel(),0,&buffer);
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"Failed to resolve pid name\n");
    // }

    // debug_printf("Received name for pid %d: %s\n",0,buffer);


    return EXIT_SUCCESS;
}
