/*
 * Copyright (c) 2009, 2011, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <stdio.h>
#include <string.h>
#include <aos/aos.h>

#include <fs/fs.h>
#include <fs/fatfs.h>
#include <drivers/sdhc.h>

#include "fs_internal.h"

//#define BULK_MEM_SIZE       (1U << 16)      // 64kB
//#define BULK_BLOCK_SIZE     BULK_MEM_SIZE   // (it's RPC)

// Directory attributes
#define ATTR_READ_ONLY ((uint8_t) 0x01))
#define ATTR_HIDDEN ((uint8_t) 0x02)
#define ATTR_SYSTEM ((uint8_t) 0x04)
#define ATTR_VOLUME_ID ((uint8_t) 0x08)
#define ATTR_DIRECTORY ((uint8_t) 0x10)
#define ATTR_ARCHIVE ((uint8_t) 0x20)
#define ATTR_LONG_NAME ((uint8_t)(ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID))
#define ATTR_LONG_NAME_MASK ((uint8_t)(DEBUG_ATTR_READ_ONLY | \
                                DEBUG_ATTR_HIDDEN | \
                                DEBUG_ATTR_SYSTEM | \
                                DEBUG_ATTR_VOLUME_ID | \
                                DEBUG_ATTR_DIRECTORY | \
                                DEBUG_ATTR_ARCHIVE))

/**
 * @brief an entry in the fatfs
 */
struct fatfs_dirent
{
    char *name;                     ///< name of the file or directory
    size_t size;                    ///< the size of the direntry in bytes or files
    //size_t refcount;                ///< reference count for open handles
    struct fatfs_dirent *parent;    ///< parent directory

    uint32_t cluster_start_sector;
    uint32_t sector;
    uint32_t byte;

    struct fatfs_dirent *next;      ///< parent directory
    struct fatfs_dirent *prev;      ///< parent directory

    bool is_dir;                    ///< flag indication this is a dir

    union {
        void *data;                 ///< file data pointer
        struct fatfs_dirent *dir;   ///< directory pointer
    };
};

struct fatfs_mount {
    struct fatfs_dirent *root;
    struct fat32_fs *fs;
    struct sdhc_s *ds;
};

/**
 * @brief a handle to the open
 */
struct fatfs_handle
{
    struct fs_handle common; // wasdas
    char *path;
    bool isdir;
    struct fatfs_dirent *dirent;

    off_t file_pos;
    struct fatfs_dirent *dir_pos;
};

const int DEVFRAME_ATTRIBUTES = KPI_PAGING_FLAGS_READ
                                | KPI_PAGING_FLAGS_WRITE
                                | KPI_PAGING_FLAGS_NOCACHE;

static errval_t initialize_sdhc_driver(struct sdhc_s **ds)
{
    errval_t err;

    struct capref argcn_frame = {
        .cnode = cnode_argcn,
        .slot = ARGCN_SLOT_DEV_0
    };

    void *device_frame;
    err = paging_map_frame_attr(get_current_paging_state(), &device_frame,
                                get_phys_size(argcn_frame), argcn_frame,
                                DEVFRAME_ATTRIBUTES, NULL, NULL);
    ON_ERR_RETURN(err);

    err = sdhc_init(ds, device_frame);
    ON_ERR_RETURN(err);

    return SYS_ERR_OK;
}

/**
 * \brief Retrieves all info concerning this fat32 partition
 */
static errval_t initialize_fat32_partition(struct sdhc_s *ds, struct fat32_fs *fs) {
    errval_t err;

    // setup buffer communication page
    size_t retbytes;
    err = frame_alloc_and_map_flags(&fs->buf_cap, SDHC_BLOCK_SIZE, &retbytes, &fs->buf_va, VREGION_FLAGS_READ_WRITE_NOCACHE);
    ON_ERR_RETURN(err);

    assert(SDHC_BLOCK_SIZE <= retbytes && "Allocated blocksize is too small");

    // Read first sector
    fs->bpb_sector = 0;
    err = sdhc_read_block(ds, fs->bpb_sector, get_phys_addr(fs->buf_cap));
    ON_ERR_RETURN(err);

    memcpy(&fs->bpb, fs->buf_va, sizeof(struct fatfs_bpb));
    assert(65525 <= (fs->bpb.totSec32 - (fs->bpb.rsvdSecCnt + (fs->bpb.numFATs * fs->bpb.fatSz32))) / fs->bpb.secPerClus && "Error, partition is not FAT32");
    assert(SDHC_BLOCK_SIZE == fs->bpb.bytsPerSec && "Error: readblocksize is unequal sectorsize");

    fs->fsinfo_sector = fs->bpb.fsInfo;
    fs->fat_sector = fs->bpb.rsvdSecCnt;
    fs->data_sector = fs->fat_sector + fs->bpb.numFATs * fs->bpb.fatSz32;
    fs->rootDir_sector = fs->data_sector + fs->bpb.secPerClus * (fs->bpb.rootClus - 2);

    // Read fsinfo sector
    err = sdhc_read_block(ds, fs->fsinfo_sector, get_phys_addr(fs->buf_cap));
    ON_ERR_RETURN(err);

    memcpy(&fs->fsi, fs->buf_va, sizeof(struct fs_info));
    assert(0x41615252 == fs->fsi.leadSig && "Error: Not an fsinfo sector");

    debug_printf(">> Bytes per Sector: %d\n", fs->bpb.bytsPerSec);
    debug_printf(">> Sector per Cluster: %d\n", fs->bpb.secPerClus);

    debug_printf(">> FATSector: %d\n", fs->fat_sector);
    debug_printf(">> DATASector: %d\n", fs->data_sector);
    debug_printf(">> RootDirSector: %d\n", fs->rootDir_sector);

    /*
    err = sdhc_read_block(sdhc_driver_state, fs->rootDir_sector, get_phys_addr(fs->buf_cap));
    ON_ERR_RETURN(err);
    for (int i = 0; i < 10; i++) {
        struct fatfs_short_dirent dir;
        memcpy(&dir, fs->buf_va+32*i, sizeof(struct fatfs_short_dirent));


        debug_printf(">> %hhu\n", dir.ord);
        debug_printf(">> %s\n", dir.name1);
        debug_printf(">> %s\n", dir.name2);
        debug_printf(">> %s\n", dir.name3);
        debug_printf(">> %hhu\n", dir.attr);
        debug_printf(">> %hhu\n", dir.type);


        debug_printf(">> Name:  %s\n", dir.name);
        debug_printf(">> Attr:  0x%x\n", dir.attr);
        debug_printf(">> cTime: %hhu\n", dir.crtTimeTenth);
        debug_printf(">> size:  %u\n", dir.fileSize);
        debug_printf(">> hi:    %hu\n", dir.fstClusHi);
        debug_printf(">> lo:    %hu\n", dir.fstClusLow);
        debug_printf(">> <<\n");
    }

    err = sdhc_read_block(ds, fs->data_sector + (3-2) * fs->bpb.secPerClus, get_phys_addr(fs->buf_cap));
    ON_ERR_RETURN(err);

    debug_printf("================= \n");
    for (int i = 0; i < 10; i++) {
        struct fatfs_short_dirent dir;
        memcpy(&dir, fs->buf_va+32*i, sizeof(struct fatfs_short_dirent));


        debug_printf(">> %hhu\n", dir.ord);
        debug_printf(">> %s\n", dir.name1);
        debug_printf(">> %s\n", dir.name2);
        debug_printf(">> %s\n", dir.name3);
        debug_printf(">> %hhu\n", dir.attr);
        debug_printf(">> %hhu\n", dir.type);


        debug_printf(">> Name:  %s\n", dir.name);
        debug_printf(">> Attr:  0x%x\n", dir.attr);
        debug_printf(">> cTime: %hhu\n", dir.crtTimeTenth);
        debug_printf(">> size:  %u\n", dir.fileSize);
        debug_printf(">> hi:    %hu\n", dir.fstClusHi);
        debug_printf(">> lo:    %hu\n", dir.fstClusLow);
        debug_printf(">> <<\n");
    }

    err = sdhc_read_block(sdhc_driver_state, fs->data_sector + (4-2) * fs->bpb.secPerClus, get_phys_addr(fs->buf_cap));
    ON_ERR_RETURN(err);

    debug_printf("================= \n");
    for (int i = 0; i < 1; i++) {
        debug_printf(">> Filesize: %lu\n", strlen((char *) fs->buf_va));
        debug_printf(">> Content: %s\n", (uint8_t *) fs->buf_va);
        debug_printf(">> <<\n");
    }

    err = sdhc_read_block(sdhc_driver_state, fs->fat_sector, get_phys_addr(fs->buf_cap));
    ON_ERR_RETURN(err);

    uint32_t fatentry;
    for (int i = 0; i < 10; i++) {
        fatentry = *(uint32_t *) (fs->buf_va+i*sizeof(uint32_t));
        debug_printf(">> fatentry %d: %x\n", i, fatentry);
    }

    err = sdhc_read_block(sdhc_driver_state, fs->data_sector + (7-2) * fs->bpb.secPerClus, get_phys_addr(fs->buf_cap));
    ON_ERR_RETURN(err);

    debug_printf("================= \n");
    for (int i = 0; i < 1; i++) {
        debug_printf(">> Filesize: %lu\n", strlen((char *) fs->buf_va));
        debug_printf(">> Content: %s\n", (uint8_t *) fs->buf_va);
        debug_printf(">> <<\n");
    }
    */



    return err;
}

//bool isDir(struct fat32_fs *fs, void *addr) {

//}

/*
void updateFreeClusterCount(void) {

}

void updateNextFreeCluster(void) {

}*/

__unused
static struct fatfs_handle *handle_open(struct fatfs_dirent *d)
{
    struct fatfs_handle *h = calloc(1, sizeof(*h));
    if (h == NULL) {
        return NULL;
    }

    //d->refcount++;
    h->isdir = d->is_dir;
    h->dirent = d;

    return h;
}

__unused
static inline void handle_close(struct fatfs_handle *h)
{
    //assert(h->dirent->refcount > 0);
    //h->dirent->refcount--;
    free(h->path);
    free(h);
}

__unused
static errval_t find_dirent(struct fatfs_mount *mount, struct fatfs_dirent *root, const char *name,
                            struct fatfs_dirent **ret_de)
{
    if (!root->is_dir) {
        return FS_ERR_NOTDIR;
    }

    struct fatfs_dirent *d = root->dir;

    while(d) {
        if (strcmp(d->name, name) == 0) {
            *ret_de = d;
            return SYS_ERR_OK;
        }

        d = d->next;
    }

    return FS_ERR_NOTFOUND;
}

__unused
static errval_t resolve_path(struct fatfs_mount *mount, const char *path,
                             struct fatfs_handle **ret_fh)
{
    errval_t err;

    struct fatfs_dirent *root = mount->root;

    // skip leading /
    size_t pos = 0;
    if (path[0] == FS_PATH_SEP) {
        pos++;
    }

    struct fatfs_dirent *next_dirent;

    while (path[pos] != '\0') {
        char *nextsep = strchr(&path[pos], FS_PATH_SEP);
        size_t nextlen;
        if (nextsep == NULL) {
            nextlen = strlen(&path[pos]);
        } else {
            nextlen = nextsep - &path[pos];
        }

        char pathbuf[nextlen + 1];
        memcpy(pathbuf, &path[pos], nextlen);
        pathbuf[nextlen] = '\0';

        err = find_dirent(mount, root, pathbuf, &next_dirent);
        if (err_is_fail(err)) {
            debug_printf("Error: Directory/File not found");
            return err;
        }

        if (!next_dirent->is_dir && nextsep != NULL) {
            return FS_ERR_NOTDIR;
        }

        root = next_dirent;
        if (nextsep == NULL) {
            break;
        }

        pos += nextlen + 1;
    }

    /* create the handle */

    if (ret_fh) {
        struct fatfs_handle *fh = handle_open(root);
        if (fh == NULL) {
            return LIB_ERR_MALLOC_FAIL;
        }

        fh->path = strdup(path);
        //fh->common.mount = root;

        *ret_fh = fh;
    }

    return SYS_ERR_OK;
}

__unused
errval_t fatfs_open(void *st, const char *path, fatfs_handle_t *rethandle)
{
    errval_t err;

    struct fatfs_mount *mount = st;

    struct fatfs_handle *handle;
    err = resolve_path(mount, path, &handle);
    if (err_is_fail(err)) {
        return err;
    }

    if (handle->isdir) {
        handle_close(handle);
        return FS_ERR_NOTFILE;
    }

    *rethandle = handle;

    return SYS_ERR_OK;
}


/*
static void dirent_remove(struct ramfs_dirent *entry)
{
    if (entry->prev == NULL) {
        */
/* entry was the first in list, update parent pointer *//*

        if (entry->parent) {
            assert(entry->parent->is_dir);
            entry->parent->dir = entry->next;
        }
    } else {
        */
/* there are entries before that one *//*

        entry->prev->next = entry->next;
    }

    if (entry->next) {
        */
/* update prev pointer *//*

        entry->next->prev = entry->prev;
    }
}

static void dirent_remove_and_free(struct ramfs_dirent *entry)
{
    dirent_remove(entry);
    free(entry->name);
    if (!entry->is_dir) {
        free(entry->data);
    }

    memset(entry, 0x00, sizeof(*entry));
    free(entry);
}

static void dirent_insert(struct ramfs_dirent *parent, struct ramfs_dirent *entry)
{
    assert(parent);
    assert(parent->is_dir);

    entry->next = NULL;
    entry->prev = NULL;
    entry->parent = parent;
    if (parent->dir) {
        entry->next = parent->dir;
        parent->dir->prev= entry;
    }

    parent->dir = entry;
}

static struct ramfs_dirent *dirent_create(const char *name, bool is_dir)
{
    struct ramfs_dirent *d = calloc(1, sizeof(*d));
    if (d == NULL) {
        return NULL;
    }

    d->is_dir = is_dir;
    d->name = strdup(name);

    return d;
}

errval_t ramfs_create(void *st, const char *path, ramfs_handle_t *rethandle)
{
    errval_t err;

    struct ramfs_mount *mount = st;

    err = resolve_path(mount->root, path, NULL);
    if (err_is_ok(err)) {
        return FS_ERR_EXISTS;
    }

    struct ramfs_handle *parent = NULL;
    const char *childname;

    // find parent directory
    char *lastsep = strrchr(path, FS_PATH_SEP);
    if (lastsep != NULL) {
        childname = lastsep + 1;

        size_t pathlen = lastsep - path;
        char pathbuf[pathlen + 1];
        memcpy(pathbuf, path, pathlen);
        pathbuf[pathlen] = '\0';

        // resolve parent directory
        err = resolve_path(mount->root, pathbuf, &parent);
        if (err_is_fail(err)) {
            return err;
        } else if (!parent->isdir) {
            return FS_ERR_NOTDIR; // parent is not a directory
        }
    } else {
        childname = path;
    }

    struct ramfs_dirent *dirent = dirent_create(childname, false);
    if (dirent == NULL) {
        return LIB_ERR_MALLOC_FAIL;
    }

    if (parent) {
        dirent_insert(parent->dirent, dirent);
        handle_close(parent);
    } else {
        dirent_insert(mount->root, dirent);
    }

    if (rethandle) {
        struct ramfs_handle *fh = handle_open(dirent);
        if (fh  == NULL) {
            return LIB_ERR_MALLOC_FAIL;
        }
        fh->path = strdup(path);
        *rethandle = fh;
    }

    return SYS_ERR_OK;
}

errval_t ramfs_remove(void *st, const char *path)
{
    errval_t err;

    struct ramfs_mount *mount = st;

    struct ramfs_handle *handle;
    err = resolve_path(mount->root, path, &handle);
    if (err_is_fail(err)) {
        return err;
    }

    if (handle->isdir) {
        return FS_ERR_NOTFILE;
    }

    struct ramfs_dirent *dirent = handle->dirent;
    if (dirent->    refcount != 1) {
        handle_close(handle);
        return FS_ERR_BUSY;
    }


    dirent_remove_and_free(dirent);

    return SYS_ERR_OK;
}

errval_t ramfs_read(void *st, ramfs_handle_t handle, void *buffer, size_t bytes,
                           size_t *bytes_read)
{
    struct ramfs_handle *h = handle;

    if (h->isdir) {
        return FS_ERR_NOTFILE;
    }

    assert(h->file_pos >= 0);

    if (h->dirent->data == NULL) {
        bytes = 0;
    } else if (h->dirent->size < h->file_pos) {
        bytes = 0;
    } else if (h->dirent->size < h->file_pos + bytes) {
        bytes = h->dirent->size - h->file_pos;
        assert(h->file_pos + bytes == h->dirent->size);
    }

    memcpy(buffer, h->dirent->data + h->file_pos, bytes);

    h->file_pos += bytes;

    *bytes_read = bytes;

    return SYS_ERR_OK;
}

errval_t ramfs_write(void *st, ramfs_handle_t handle, const void *buffer,
                            size_t bytes, size_t *bytes_written)
{
    struct ramfs_handle *h = handle;
    assert(h->file_pos >= 0);

    size_t offset = h->file_pos;

    if (h->isdir) {
        return FS_ERR_NOTFILE;
    }

    if (h->dirent->size < offset + bytes) {
        */
/* need to realloc the buffer *//*

        void *newbuf = realloc(h->dirent->data, offset + bytes);
        if (newbuf == NULL) {
            return LIB_ERR_MALLOC_FAIL;
        }
        h->dirent->data = newbuf;
    }

    memcpy(h->dirent->data + offset, buffer, bytes);

    if (bytes_written) {
        *bytes_written = bytes;
    }

    h->file_pos += bytes;
    h->dirent->size += bytes;

    return SYS_ERR_OK;
}


errval_t ramfs_truncate(void *st, ramfs_handle_t handle, size_t bytes)
{
    struct ramfs_handle *h = handle;

    if (h->isdir) {
        return FS_ERR_NOTFILE;
    }

    void *newdata = realloc(h->dirent->data, bytes);
    if (newdata == NULL) {
        return LIB_ERR_MALLOC_FAIL;
    }
    h->dirent->data = newdata;
    h->dirent->size = bytes;

    return SYS_ERR_OK;
}

errval_t ramfs_tell(void *st, ramfs_handle_t handle, size_t *pos)
{
    struct ramfs_handle *h = handle;
    if (h->isdir) {
        *pos = 0;
    } else {
        *pos = h->file_pos;
    }
    return SYS_ERR_OK;
}

errval_t ramfs_stat(void *st, ramfs_handle_t inhandle, struct fs_fileinfo *info)
{
    struct ramfs_handle *h = inhandle;

    assert(info != NULL);
    info->type = h->isdir ? FS_DIRECTORY : FS_FILE;
    info->size = h->dirent->size;

    return SYS_ERR_OK;
}

errval_t ramfs_seek(void *st, ramfs_handle_t handle, enum fs_seekpos whence,
                     off_t offset)
{
    struct ramfs_handle *h = handle;
    struct fs_fileinfo info;
    errval_t err;

    switch (whence) {
    case FS_SEEK_SET:
        assert(offset >= 0);
        if (h->isdir) {
            h->dir_pos = h->dirent->parent->dir;
            for (size_t i = 0; i < offset; i++) {
                if (h->dir_pos  == NULL) {
                    break;
                }
                h->dir_pos = h->dir_pos->next;
            }
        } else {
            h->file_pos = offset;
        }
        break;

    case FS_SEEK_CUR:
        if (h->isdir) {
            assert(!"NYI");
        } else {
            assert(offset >= 0 || -offset <= h->file_pos);
            h->file_pos += offset;
        }

        break;

    case FS_SEEK_END:
        if (h->isdir) {
            assert(!"NYI");
        } else {
            err = ramfs_stat(st, handle, &info);
            if (err_is_fail(err)) {
                return err;
            }
            assert(offset >= 0 || -offset <= info.size);
            h->file_pos = info.size + offset;
        }
        break;

    default:
        USER_PANIC("invalid whence argument to ramfs seek");
    }

    return SYS_ERR_OK;
}

errval_t ramfs_close(void *st, ramfs_handle_t inhandle)
{
    struct ramfs_handle *handle = inhandle;
    if (handle->isdir) {
        return FS_ERR_NOTFILE;
    }
    handle_close(handle);
    return SYS_ERR_OK;
}

errval_t ramfs_opendir(void *st, const char *path, ramfs_handle_t *rethandle)
{
    errval_t err;

    struct ramfs_mount *mount = st;

    struct ramfs_handle *handle;
    err = resolve_path(mount->root, path, &handle);
    if (err_is_fail(err)) {
        return err;
    }

    if (!handle->isdir) {
        handle_close(handle);
        return FS_ERR_NOTDIR;
    }

    handle->dir_pos = handle->dirent->dir;

    *rethandle = handle;

    return SYS_ERR_OK;
}

errval_t ramfs_dir_read_next(void *st, ramfs_handle_t inhandle, char **retname,
                              struct fs_fileinfo *info)
{
    struct ramfs_handle *h = inhandle;

    if (!h->isdir) {
        return FS_ERR_NOTDIR;
    }

    struct ramfs_dirent *d = h->dir_pos;
    if (d == NULL) {
        return FS_ERR_INDEX_BOUNDS;
    }


    if (retname != NULL) {
        *retname = strdup(d->name);
    }

    if (info != NULL) {
        info->type = d->is_dir ? FS_DIRECTORY : FS_FILE;
        info->size = d->size;
    }

    h->dir_pos = d->next;

    return SYS_ERR_OK;
}

errval_t ramfs_closedir(void *st, ramfs_handle_t dhandle)
{
    struct ramfs_handle *handle = dhandle;
    if (!handle->isdir) {
        return FS_ERR_NOTDIR;
    }

    free(handle->path);
    free(handle);

    return SYS_ERR_OK;
}

// fails if already present
errval_t ramfs_mkdir(void *st, const char *path)
{
    errval_t err;

    struct ramfs_mount *mount = st;

    err = resolve_path(mount->root, path, NULL);
    if (err_is_ok(err)) {
        return FS_ERR_EXISTS;
    }


    struct ramfs_handle *parent  = NULL;
    const char *childname;

    // find parent directory
    char *lastsep = strrchr(path, FS_PATH_SEP);
    if (lastsep != NULL) {
        childname = lastsep + 1;

        size_t pathlen = lastsep - path;
        char pathbuf[pathlen + 1];
        memcpy(pathbuf, path, pathlen);
        pathbuf[pathlen] = '\0';

        // resolve parent directory
        err = resolve_path(mount->root, pathbuf, &parent);
        if (err_is_fail(err)) {
            handle_close(parent);
            return err;
        } else if (!parent->isdir) {
            handle_close(parent);
            return FS_ERR_NOTDIR; // parent is not a directory
        }
    } else {
        childname = path;
    }

    struct ramfs_dirent *dirent = dirent_create(childname, true);
    if (dirent == NULL) {
        return LIB_ERR_MALLOC_FAIL;
    }

    if (parent) {
        dirent_insert(parent->dirent, dirent);
        handle_close(parent);
    } else {
        dirent_insert(mount->root, dirent);
    }

    return SYS_ERR_OK;
}



errval_t ramfs_rmdir(void *st, const char *path)
{
    errval_t err;

    struct ramfs_mount *mount = st;

    struct ramfs_handle *handle;
    err = resolve_path(mount->root, path, &handle);
    if (err_is_fail(err)) {
        return err;
    }

    if (!handle->isdir) {
        goto out;
        err =  FS_ERR_NOTDIR;
    }

    if (handle->dirent->refcount != 1) {
        handle_close(handle);
        return FS_ERR_BUSY;
    }

    assert(handle->dirent->is_dir);

    if (handle->dirent->dir) {
        err = FS_ERR_NOTEMPTY;
        goto out;
    }

    dirent_remove_and_free(handle->dirent);

    out:
    free(handle);

    return err;
}

*/
errval_t fatfs_mount(const char *path, fatfs_mount_t *retst)
{
    /* Setup channel and connect ot service */
    /* TODO: setup channel to init for multiboot files */

    errval_t err;

    // Init driver state
    struct sdhc_s *ds;
    err = initialize_sdhc_driver(&ds);
    if (err_is_fail(err)) {
        free(ds);
        debug_printf("Fatal Error, could not initialize SHDC driver\n");
        abort();
    }

    // Init fat32 filesystem state
    struct fat32_fs *fs = calloc(1, sizeof(struct fat32_fs));
    if (fs == NULL) {
        free(ds);
        return LIB_ERR_MALLOC_FAIL;
    }
    initialize_fat32_partition(ds, fs);

    // Create mount point root
    struct fatfs_mount *mount = calloc(1, sizeof(struct fatfs_mount));
    if (mount == NULL) {
        free(ds);
        free(fs);
        return LIB_ERR_MALLOC_FAIL;
    }

    struct fatfs_dirent *fatfs_root;
    fatfs_root = calloc(1, sizeof(*fatfs_root));
    if (fatfs_root == NULL) {
        free(ds);
        free(fs);
        free(mount);
        return LIB_ERR_MALLOC_FAIL;
    }

    fatfs_root->size = 0;
    fatfs_root->is_dir = true;
    fatfs_root->name = "/";
    fatfs_root->parent = NULL;
    fatfs_root->cluster_start_sector = fs->rootDir_sector;
    fatfs_root->sector = fs->rootDir_sector;
    fatfs_root->byte = 0;

    mount->root = fatfs_root;

    *retst = mount;

    return SYS_ERR_OK;
}