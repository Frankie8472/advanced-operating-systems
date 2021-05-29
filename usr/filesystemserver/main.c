/**
 * \file
 * \brief Filesystemserver
 */

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
#include <stdlib.h>
#include <fs/fs.h>
#include <fs/dirent.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>

__unused
static errval_t test_read_dir(char *dir)
{
    errval_t err;

    fs_dirhandle_t dh;
    err = opendir(dir, &dh);
    if (err_is_fail(err)) {
        return err;
    }

    assert(dh);

    do {
        char *name;
        err = readdir(dh, &name);
        if (err_no(err) == FS_ERR_INDEX_BOUNDS) {
            break;
        } else if (err_is_fail(err)) {
            goto err_out;
        }
        printf("%s\n", name);
    } while(err_is_ok(err));

    return closedir(dh);
    err_out:
    return err;
}

__unused
static errval_t test_fread(char *file)
{
    int res = 0;

    FILE *f = fopen(file, "r");
    if (f == NULL) {
        return FS_ERR_OPEN;
    }

    /* obtain the file size */
    res = fseek (f , 0 , SEEK_END);
    if (res) {
        return FS_ERR_INVALID_FH;
    }

    size_t filesize = ftell (f);
    rewind (f);

    printf("File size is %zu\n", filesize);

    char *buf = calloc(filesize + 2, sizeof(char));
    if (buf == NULL) {
        return LIB_ERR_MALLOC_FAIL;
    }

    size_t read = fread(buf, 1, filesize, f);

    printf("read: %s\n", buf);

    if (read != filesize) {
        return FS_ERR_READ;
    }

    rewind(f);

    size_t nchars = 0;
    int c;
    do {
        c = fgetc (f);
        nchars++;
    } while (c != EOF);

    if (nchars < filesize) {
        return FS_ERR_READ;
    }

    free(buf);
    res = fclose(f);
    if (res) {
        return FS_ERR_CLOSE;
    }

    return SYS_ERR_OK;
}

__unused
static errval_t test_fwrite(char *file)
{
    int res = 0;

    FILE *f = fopen(file, "w");
    if (f == NULL) {
        return FS_ERR_OPEN;
    }

    const char *inspirational_quote = "I love deadlines. I like the whooshing "
                                      "sound they make as they fly by.";

    size_t written = fwrite(inspirational_quote, 1, strlen(inspirational_quote),
                            f);
    printf("wrote %zu bytes\n", written);

    if (written != strlen(inspirational_quote)) {
        return FS_ERR_READ;
    }

    res = fclose(f);
    if (res) {
        return FS_ERR_CLOSE;
    }

    return SYS_ERR_OK;
}

int main(int argc, char *argv[])
{
    errval_t err;
    err = filesystem_init();
    if (err_is_fail(err)){
        return EXIT_FAILURE;
    }

    debug_printf(">> OPEN/CREATE FILE\n");
    FILE *f = fopen("/sdcard/ASDF.TXT", "wr");
    fwrite("HELLO SALADBAR", 14, 1, f);
    char ret[20];
    fread(ret, 1, 20, f);
    debug_printf(">> READ: |%s|\n", ret);

    debug_printf(">> OPEN/CREATE FOLDER\n");
    mkdir("/sdcard/FOLDER");
    debug_printf(">> READ FOLDER\n");
    test_read_dir("/sdcard");
    test_read_dir("/sdcard/FOLDER");
    test_read_dir("/sdcard/FOLDER/.");
    test_read_dir("/sdcard/FOLDER/..");

/*
    debug_printf(">> OPEN FILE\n");

    FILE* f = fopen("/SHORT.TXT", "r");

    if (f != 0x0) {
        debug_printf(">> READ FILE 0x%x\n", f);
        char ret[2500];
        debug_printf(">> 1\n");
        size_t len = fread(ret, 1, 2500, f);
        //ret[2500] = '\0';
        debug_printf(">> CONTLEN: %d \n", len);
        debug_printf(">> CONTENT: %s \n", ret);
        debug_printf("GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAM1OLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUA2CAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOL3E GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE4 GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GU5ACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACAMOLE GUACATROLOLO\n");
        debug_printf(">> CLOSE FILE\n");
        fclose(f);
    } else {
        debug_printf(">> FILE NOT FOUND\n");
    }
*/
    return EXIT_SUCCESS;
}
