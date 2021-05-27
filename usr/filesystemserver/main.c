/**
 * \file
 * \brief Filesystemserver
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
#include <fs/fs.h>

int main(int argc, char *argv[])
{
    errval_t err;
    printf(">> REACHED\n");

    err = filesystem_init();
    if (err_is_fail(err)){
        return EXIT_FAILURE;
    }
    debug_printf(">> OPEN FILE\n");
    //FILE* f = fopen("/ASDF.TXT", "r");
    FILE* f = fopen("/LONG.TXT", "r");
    if (f != 0x0) {
        debug_printf(">> READ FILE 0x%x\n", f);
        char ret[20];
        fread(ret, 1, 19, f);
        ret[19] = '\0';
        debug_printf(">> CONTENT: %s\n", ret);
        debug_printf(">> CLOSE FILE\n");
        fclose(f);
    } else {
        debug_printf(">> FILE NOT FOUND\n");
    }
    debug_printf(">> DONE\n");

    return EXIT_SUCCESS;
}
