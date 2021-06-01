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
#include <aos/nameserver.h>
#include <aos/fs_service.h>

#include <aos/default_interfaces.h>


__unused
static void file_read(char *path, size_t size, char **ret)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        debug_printf("Error: file not found\n");
        return;
    }

    char *content = calloc(1, size);
    fread(content, 1, size, f);
    content[size+1] = '\0';
    fclose(f);
    *ret = content;
}


__unused
static void file_write(char *path, char *buffer, size_t size)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        debug_printf("Error: file not found\n");
        return;
    }

    fwrite(buffer, 1, size, f);
    fclose(f);
}

__unused
static void file_delete(char *path)
{
    rm(path);
}

__unused
static void dir_read(char *path, char **ret)
{
    errval_t err;
    fs_dirhandle_t dh;
    err = opendir(path, &dh);
    if (err_is_fail(err)) {
        return;
    }

    assert(dh);

    char *str = calloc(1, 1000);
    str[0] = '\0';

    do {
        char *name;
        err = readdir(dh, &name);
        if (err_no(err) == FS_ERR_INDEX_BOUNDS) {
            break;
        } else if (err_is_fail(err)) {
            closedir(dh);
            return;
        }
        strcat(str, name);
        strcat(str, "\n");
    } while(err_is_ok(err));
    closedir(dh);

    *ret = str;
}

__unused
static void dir_create(char *path)
{
    mkdir(path);
}

__unused
static void dir_delete(char *path)
{
    rmdir(path);
}

__unused
static void elf_file_spawn(char* path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        debug_printf("Error: file not found\n");
        return;
    }
    struct fs_fileinfo fsinfo;
    errval_t err = fstat(f, &fsinfo);
    if (err_is_fail(err)){
        debug_printf("Error: fstat\n");
        return;
    }
    char *content = calloc(1, fsinfo.size);
    fread(content, 1, fsinfo.size, f);
    fclose(f);

    // rpc call to init and call spawn load by name with elf
    debug_printf("NYI\n");
    free(content);
}

static void server_recv_handler(void *stptr, void *message,
                                size_t bytes,
                                void **response, size_t *response_bytes,
                                struct capref rx_cap, struct capref *tx_cap)
{
    debug_printf(">> REEEEEECH ============\n");
    struct fs_service_message *fsm = (struct fs_service_message *) message;

    size_t path_size = fsm->path_size;
    size_t data_size = fsm->data_size;

    char path[path_size];
    strcpy(path, fsm->data);

    *response_bytes = 0;

    switch (fsm->type) {
    case F_READ: {
        file_read(path, data_size, (char **) response);
        *response_bytes = strlen(*response);
        break;
    }
    case F_WRITE: {
        char data[data_size];
        strcpy(data, fsm->data + path_size);  // TODO +1?
        file_write(path, data, data_size);
        break;
    }
    case F_RM: {
        file_delete(path);
        break;
    }
    case D_READ: {
        dir_read(path, (char **) response);
        *response_bytes = strlen(*response);
        break;
    }
    case D_MKDIR: {
        dir_create(path);
        break;
    }
    case D_RM: {
        dir_delete(path);
        break;
    }
    case SPAWN_ELF: {
        elf_file_spawn(path);
        break;
    }
    default:
        break;
    }
}

int main(int argc, char *argv[])
{
    debug_printf(">> HELLO FS NAMESERVER ==============\n");
    errval_t err;
    err = filesystem_init();
    if (err_is_fail(err)){
        return EXIT_FAILURE;
    }
    debug_printf(">> LINK FS NAMESERVER ==============\n");
    // NAMESERVER LINK
    err = nameservice_register_properties("/fs", server_recv_handler, NULL, true,"type=fs");
    if (err_is_fail(err)){
        return EXIT_FAILURE;
    }
    debug_printf(">> WHILE FS NAMESERVER ==============\n");
    while(true){
        err = event_dispatch(get_default_waitset());
        if (err == LIB_ERR_NO_EVENT){
            thread_yield();
        }
    }


    err = aos_rpc_call(get_init_rpc(), INIT_FS_ON);
    return EXIT_SUCCESS;

    /*
    debug_printf(">> OPEN/CREATE FILE1\n");
    FILE *g = fopen("/sdcard/ASDF.TXT", "w");

    debug_printf(">> WRITE FILE\n");
    fwrite("HELLO SALADBAR", 14, 1, g);
    fclose(g);

    debug_printf(">> OPEN FILE2\n");
    FILE *f = fopen("/sdcard/ASDF.TXT", "r");

    debug_printf(">> READ FILE\n");
    char ret[20];
    fread(ret, 1, 20, f);
    debug_printf(">> READ: |%s|\n", ret);

    debug_printf(">> OPEN/CREATE FOLDER\n");
    mkdir("/sdcard/FOLDER");

    debug_printf(">> READ FOLDER\n");
    test_read_dir("/sdcard/FOLDER");

    return EXIT_SUCCESS;
*/
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
