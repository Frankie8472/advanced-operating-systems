#include <stdint.h>
#include <stdlib.h>
#include <aos/fs_service.h>

void init_fs(void)
{
    errval_t err;
    nameservice_chan_t _nschan;

    err = nameservice_lookup(FS_SERVICE_NAME, &_nschan);
    while(err_is_fail(err)){
        err = nameservice_lookup(FS_SERVICE_NAME, &_nschan);
        thread_yield();
    }
}

void read_file(char *path, size_t size, char *ret)
{
    errval_t err;
    nameservice_chan_t _nschan;

    err = nameservice_lookup(FS_SERVICE_NAME, &_nschan);
    if(err_is_fail(err)) {
        debug_printf("not found\n");
        return;
    }

    struct fs_service_message *fsm = malloc(sizeof(struct fs_service_message) + strlen(path));
    fsm->type = F_READ;
    fsm->path_size = strlen(path);
    fsm->data_size = size;
    strcpy(fsm->data, path);

    void *response;
    size_t response_bytes;

    err = nameservice_rpc(_nschan, (void *) fsm, strlen((char *) fsm),
                                   &response, &response_bytes,
                                   NULL_CAP, NULL_CAP);
    strcpy(ret, response);
    free(response);
    free(fsm);

    if (err_is_fail(err)) {
        DEBUG_ERR(err, "failed to do the nameservice rpc\n");
    }
}

void write_file(char *path, char *data)
{
    errval_t err;
    nameservice_chan_t _nschan;

    err = nameservice_lookup(FS_SERVICE_NAME, &_nschan);
    if(err_is_fail(err)) {
        debug_printf("not found\n");
        return;
    }

    struct fs_service_message *fsm = malloc(sizeof(struct fs_service_message) + strlen(path) + strlen(data));
    fsm->type = F_WRITE;
    fsm->path_size = strlen(path);
    fsm->data_size = strlen(data);
    strcpy(fsm->data, path);
    strcpy(fsm->data, data);

    void *response;
    size_t response_bytes;

    err = nameservice_rpc(_nschan, (void *) fsm, strlen((char *) fsm),
                          &response, &response_bytes,
                          NULL_CAP, NULL_CAP);
    free(fsm);

    if (err_is_fail(err)) {
        DEBUG_ERR(err, "failed to do the nameservice rpc\n");
    }
}

void delete_file(char *path)
{
    errval_t err;
    nameservice_chan_t _nschan;

    err = nameservice_lookup(FS_SERVICE_NAME, &_nschan);
    if(err_is_fail(err)) {
        debug_printf("not found\n");
        return;
    }

    struct fs_service_message *fsm = malloc(sizeof(struct fs_service_message) + strlen(path));
    fsm->type = F_RM;
    fsm->path_size = strlen(path);
    fsm->data_size = 0;
    strcpy(fsm->data, path);

    void *response;
    size_t response_bytes;

    err = nameservice_rpc(_nschan, (void *) fsm, strlen((char *) fsm),
                          &response, &response_bytes,
                          NULL_CAP, NULL_CAP);
    free(fsm);

    if (err_is_fail(err)) {
        DEBUG_ERR(err, "failed to do the nameservice rpc\n");
    }
}

void read_dir(char *path, char **ret)
{
    errval_t err;
    nameservice_chan_t _nschan;

    err = nameservice_lookup(FS_SERVICE_NAME, &_nschan);
    if(err_is_fail(err)) {
        debug_printf("not found\n");
        return;
    }

    struct fs_service_message *fsm = malloc(sizeof(struct fs_service_message) + strlen(path));
    fsm->type = D_READ;
    fsm->path_size = strlen(path);
    fsm->data_size = 0;
    strcpy(fsm->data, path);

    void *response;
    size_t response_bytes;

    err = nameservice_rpc(_nschan, (void *) fsm, strlen((char *) fsm),
                          &response, &response_bytes,
                          NULL_CAP, NULL_CAP);
    *ret = response;
    free(fsm);

    if (err_is_fail(err)) {
        DEBUG_ERR(err, "failed to do the nameservice rpc\n");
    }
}

void create_dir(char *path)
{
    errval_t err;
    nameservice_chan_t _nschan;

    err = nameservice_lookup(FS_SERVICE_NAME, &_nschan);
    if(err_is_fail(err)) {
        debug_printf("not found\n");
        return;
    }

    struct fs_service_message *fsm = malloc(sizeof(struct fs_service_message) + strlen(path));
    fsm->type = D_MKDIR;
    fsm->path_size = strlen(path);
    fsm->data_size = 0;
    strcpy(fsm->data, path);

    void *response;
    size_t response_bytes;

    err = nameservice_rpc(_nschan, (void *) fsm, strlen((char *) fsm),
                          &response, &response_bytes,
                          NULL_CAP, NULL_CAP);
    free(fsm);

    if (err_is_fail(err)) {
        DEBUG_ERR(err, "failed to do the nameservice rpc\n");
    }
}

void delete_dir(char *path)
{
    errval_t err;
    nameservice_chan_t _nschan;

    err = nameservice_lookup(FS_SERVICE_NAME, &_nschan);
    if(err_is_fail(err)) {
        debug_printf("not found\n");
        return;
    }

    struct fs_service_message *fsm = malloc(sizeof(struct fs_service_message) + strlen(path));
    fsm->type = D_RM;
    fsm->path_size = strlen(path);
    fsm->data_size = 0;
    strcpy(fsm->data, path);

    void *response;
    size_t response_bytes;

    err = nameservice_rpc(_nschan, (void *) fsm, strlen((char *) fsm),
                          &response, &response_bytes,
                          NULL_CAP, NULL_CAP);
    free(fsm);

    if (err_is_fail(err)) {
        DEBUG_ERR(err, "failed to do the nameservice rpc\n");
    }
}


void spawn_elf_file(char* path)
{
    errval_t err;
    nameservice_chan_t _nschan;

    err = nameservice_lookup(FS_SERVICE_NAME, &_nschan);
    if(err_is_fail(err)) {
        debug_printf("not found\n");
        return;
    }

    struct fs_service_message *fsm = malloc(sizeof(struct fs_service_message) + strlen(path));
    fsm->type = SPAWN_ELF;
    fsm->path_size = strlen(path);
    fsm->data_size = 0;
    strcpy(fsm->data, path);

    void *response;
    size_t response_bytes;

    err = nameservice_rpc(_nschan, (void *) fsm, strlen((char *) fsm),
                          &response, &response_bytes,
                          NULL_CAP, NULL_CAP);
    free(fsm);

    if (err_is_fail(err)) {
        DEBUG_ERR(err, "failed to do the nameservice rpc\n");
    }
}