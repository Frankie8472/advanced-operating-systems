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
#include <stdio.h>
#include <aos/aos.h>

int main(int argc, char *argv[])
{
    printf("Hello, world! from userspace\n");
    printf("I am running on core %d :)\n", disp_get_current_core_id());
    

    for(int i = 0; i < argc; ++i){
        printf("argv[%d] = %s\n",i,argv[i]);
    }

    return EXIT_SUCCESS;
}
