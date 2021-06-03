/**
 * \file
 * \brief Hello world application
 */

#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>

#include <aos/aos_rpc.h>
#include <aos/systime.h>
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
#include <aos/fs_service.h>

int main(int argc, char *argv[])
{
    printf("Hello, world! from userspace\n");
    printf("I am running on core %d :)\n", disp_get_current_core_id());
    

    for(int i = 0; i < argc; ++i){
        printf("argv[%d] = %s\n",i,argv[i]);
    }

    debug_printf("> START FS DEMO\n");

    char *path = "/sdcard/RABA.TXT";
    char *text = "BOIIIIII123456789123456791234567890AASDADSFADFJAJKSFHJASHDFHASFDHHGGFHGFHFHFHFDFGEWGHSGfEND";

    debug_printf(">> WRITE FILE\n");
    debug_printf(">>> path: %s\n", path);
    debug_printf(">>> text: %s\n", text);

    write_file(path, text);

    debug_printf(">> READ FILE\n");
    int readsize = 20;
    char read[readsize];
    read[readsize + 1] = '\0';
    read_file(path, readsize, read);
    debug_printf(">>> ret: %s\n", read);

    debug_printf(">> DELETE FILE\n");
    delete_file(path);

    debug_printf(">> READ FILE\n");
    read_file(path, readsize, read);

    debug_printf(">> CREATE DIR \n");
    path = "/sdcard/DAFOLDER";
    create_dir(path);

    debug_printf(">> READ DIR \n");
    char *readdir;
    read_dir(path, &readdir);
    debug_printf(">> dir: %s\n", readdir);

    debug_printf(">> DELETE DIR \n");
    delete_dir(path);

    debug_printf(">> READ DIR \n");
    read_dir(path, &readdir);

    debug_printf("> END FS DEMO\n");
    return EXIT_SUCCESS;

    systime_t start, end;
    uint64_t diff;

    start = systime_now();
    for (volatile int i = 0; i < 3000000; i++);
    end = systime_now();
    diff = systime_to_us(end - start)/1000;
    debug_printf(">> loop ms: %lu\n", diff);

    debug_printf(">> Sequential reads:\n");
    for(int i = 0; i < 10; i++) {
        ///
        char *p = "/sdcard/RABA.TXT";
        char *t = "BOIIIIII123456789123456791234567890AASDADSFADFJAJKSFHJASHDFHASFDHHGGFHGFHFHFHFDFGEWGHSGfEND";
        start = systime_now();
        write_file(p, t);
        end = systime_now();
        diff = systime_to_us(end - start)/1000;
        debug_printf(">> fwrite ms: %lu\n", diff);
        ///
        int rsize = 50;
        char rd[rsize];
        start = systime_now();
        read_file(p, rsize, rd);
        end = systime_now();
        diff = systime_to_us(end - start)/1000;
        debug_printf(">> fread ms: %lu\n", diff);
        ///
        start = systime_now();
        delete_file(p);
        end = systime_now();
        diff = systime_to_us(end - start)/1000;
        debug_printf(">> rm ms: %lu\n", diff);
        ///
        p = "/sdcard/DAFOLDER";
        start = systime_now();
        create_dir(p);
        end = systime_now();
        diff = systime_to_us(end - start)/1000;
        debug_printf(">> mkdir ms: %lu\n", diff);
        ///
        char *rdd;
        start = systime_now();
        read_dir(p, &rdd);
        end = systime_now();
        free(rdd);
        diff = systime_to_us(end - start)/1000;
        debug_printf(">> readdir ms: %lu\n", diff);
        ///
        start = systime_now();
        delete_dir(p);
        end = systime_now();
        diff = systime_to_us(end - start)/1000;
        debug_printf(">> rmdir ms: %lu\n", diff);
    }

    return EXIT_SUCCESS;
}
