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
#include <aos/cache.h>

#include <fs/fs.h>
#include <fs/fatfs.h>
#include <drivers/sdhc.h>
#include <aos/systime.h>

#include "fs_internal.h"

//#define BULK_MEM_SIZE       (1U << 16)      // 64kB
//#define BULK_BLOCK_SIZE     BULK_MEM_SIZE   // (it's RPC)

#define WAIT_TIME 3000000
// Directory attributes
#define ATTR_READ_ONLY ((uint8_t) 0x01)
#define ATTR_HIDDEN ((uint8_t) 0x02)
#define ATTR_SYSTEM ((uint8_t) 0x04)
#define ATTR_VOLUME_ID ((uint8_t) 0x08)
#define ATTR_DIRECTORY ((uint8_t) 0x10)
#define ATTR_ARCHIVE ((uint8_t) 0x20)
#define ATTR_LONG_NAME ((uint8_t)(ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID))
#define ATTR_LONG_NAME_MASK ((uint8_t)(ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID | ATTR_DIRECTORY | ATTR_ARCHIVE))

void measurments(struct fatfs_mount *mount);

/**
 * @brief an entry in the fatfs
 */
struct fatfs_dirent
{
    char *name;                     ///< name of the file or directory
    size_t size;                    ///< the size of the direntry in bytes or files
    //size_t refcount;                ///< reference count for open handles
    struct fatfs_dirent *parent;    ///< parent directory

    uint32_t cluster;
    uint32_t sector;
    uint32_t sector_offset;         ///< Offset of sector to entry in bytes
    uint32_t content_cluster;
    //struct fatfs_dirent *next;      ///< parent directory
    //struct fatfs_dirent *prev;      ///< parent directory

    bool is_dir;                    ///< flag indication this is a dir
    /*
    union {
        void *data;                 ///< file data pointer
        struct fatfs_dirent *dir;   ///< directory pointer
    };*/
};

/**
 * @brief a handle to the open
 */
struct fatfs_handle
{
    struct fs_handle common;    ///< fatfs_mount pointer
    char *path;
    bool isdir;
    struct fatfs_dirent *dirent;

    off_t file_pos;    ///< offset in bytes
    off_t dir_pos;     ///< offset in bytes
};

const int DEVFRAME_ATTRIBUTES = KPI_PAGING_FLAGS_READ
                                | KPI_PAGING_FLAGS_WRITE
                                | KPI_PAGING_FLAGS_NOCACHE;


static errval_t long_sdhc_read(struct sdhc_s *ds, int index, lpaddr_t dest){
    errval_t err = sdhc_read_block(ds, index, dest);
    for(volatile int t = 0; t < WAIT_TIME; t++);
    return err;
}

static errval_t long_sdhc_write(struct sdhc_s *ds, int index, lpaddr_t dest){
    for(volatile int t = 0; t < WAIT_TIME; t++);
    errval_t err = sdhc_write_block(ds, index, dest);
    return err;
}

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

static void pathname_to_fat32name(const char *pathname, char *fat32name)
{
    char *str = fat32name;
    uint32_t limit = strlen(pathname)-1;
    assert(limit <= 11);
    for (int i = 0; i < 11; i++) {
        if (i > limit) {
            str[i] = 0x20;
        } else {
            char current = pathname[i];
            if (current == 0x2E) {
                for (int j = i; j < 8; j++) {
                    str[j] = 0x20;
                }
                str[8] = pathname[i+1];
                str[9] = pathname[i+2];
                str[10] = pathname[i+3];
                break;
            } else {
                str[i] = pathname[i];
            }
        }

    }
}

static errval_t set_cluster_zero(struct fatfs_mount *mount, uint32_t cluster) {
    errval_t err;

    // Set buf page (512B) to zero
    for (int i = 0; i < 64; i++) {
        ((uint64_t *) mount->fs->buf_va)[i] = 0;
    }

    uint32_t start_sector = mount->fs->data_sector + (cluster - 2) * mount->fs->bpb.secPerClus;

    // Iterate through the full cluster and set it 0 (the buf page)
    for (int i = 0; i < mount->fs->bpb.secPerClus; i++) {
        int sector = (int) (start_sector + i);
        err = long_sdhc_write(mount->ds, sector, get_phys_addr(mount->fs->buf_cap));
        ON_ERR_RETURN(err);
    }

    return SYS_ERR_OK;
}

static errval_t get_free_fat_entry(struct fatfs_mount *mount, uint32_t *ret){
    errval_t err;
    uint16_t fat_sec = (int) mount->fs->fat_sector;

    // Iterate over FAT cluster
    for (int i = 0; i < mount->fs->bpb.secPerClus; i++) {
        err = long_sdhc_read(mount->ds, fat_sec + i, get_phys_addr(mount->fs->buf_cap));
        ON_ERR_RETURN(err);
        uint32_t *addr = mount->fs->buf_va;
        //for(volatile int t = 0; t < 1000000; t++);
        //debug_printf(">>> FAT ADDR ONE: %x\n", *addr);

        // Iterate over FAT sector
        for (int j = 0; j < 128; j++) {
            //debug_printf(">>> FAT SECTOR: %d\n", fat_sec);
            //debug_printf(">>> FAT SECTOR i: %d\n", i);
            //debug_printf(">>> FAT SECTOR j: %d\n", j);
            //debug_printf(">>> FAT ENTRY: %x\n", addr[j]);
            //debug_printf(">>> FAT ADDR: %x\n", *((uint32_t *) mount->fs->buf_va));
            uint32_t entry = addr[j] & 0x0fffffff;
            //debug_printf(">>> FAT ENTRY: %x\n", entry);
            if (entry != 0x0ffffff8 && entry != 0x0fffffff && entry == 0x00000000) {
                addr[j] = -1;
                uint32_t new_cluster = j + (128 * i);

                // Write back assigned FAT entry (value -1)
                err = long_sdhc_write(mount->ds, fat_sec + i, get_phys_addr(mount->fs->buf_cap));
                ON_ERR_RETURN(err);

                // Set new cluster to zero
                err = set_cluster_zero(mount, new_cluster);
                ON_ERR_RETURN(err);
                //debug_printf(">>> new cluster: %d\n", new_cluster);
                *ret = new_cluster;
                return SYS_ERR_OK;
            }
        }
    }
    return FS_ERR_INDEX_BOUNDS;
}

static errval_t get_next_fat_entry(struct fatfs_mount *mount, uint32_t cur, uint32_t *ret) {
    errval_t err;
    uint16_t fat_sec = (int) (mount->fs->fat_sector + (cur / 128));

    err = long_sdhc_read(mount->ds, fat_sec, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);

    *ret = (((uint32_t *) mount->fs->buf_va)[cur % 128]) & 0x0fffffff;

    return SYS_ERR_OK;
}

static errval_t insert_new_fat_link(struct fatfs_mount *mount, size_t parent, size_t new){
    errval_t err;
    uint16_t fat_sec = (int) (mount->fs->fat_sector + (parent / 128));

    err = long_sdhc_read(mount->ds, fat_sec, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);

    ((uint32_t *) mount->fs->buf_va)[parent % 128] = new;

    err = long_sdhc_write(mount->ds, fat_sec, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);

    return SYS_ERR_OK;
}

static errval_t initialize_fat32_partition(struct sdhc_s *ds, struct fat32_fs *fs) {
    errval_t err;

    // Setup buffer communication page
    size_t retbytes;
    err = frame_alloc_and_map_flags(&fs->buf_cap, SDHC_BLOCK_SIZE, &retbytes, &fs->buf_va, VREGION_FLAGS_READ_WRITE_NOCACHE);
    ON_ERR_RETURN(err);

    assert(SDHC_BLOCK_SIZE <= retbytes && "Allocated blocksize is too small");

    // Read first sector
    fs->bpb_sector = 0;
    arm64_dcache_wbinv_range((vm_offset_t) fs->buf_va, 4096);
    //dmb();
    err = long_sdhc_read(ds, fs->bpb_sector, get_phys_addr(fs->buf_cap));
    ON_ERR_RETURN(err);

    memcpy(&fs->bpb, fs->buf_va, sizeof(struct fatfs_bpb));
    assert(65525 <= (fs->bpb.totSec32 - (fs->bpb.rsvdSecCnt + (fs->bpb.numFATs * fs->bpb.fatSz32))) / fs->bpb.secPerClus && "Error, partition is not FAT32");
    assert(SDHC_BLOCK_SIZE == fs->bpb.bytsPerSec && "Error: readblocksize is unequal sectorsize");

    fs->fsinfo_sector = fs->bpb.fsInfo;
    fs->fat_sector = fs->bpb.rsvdSecCnt;
    fs->data_sector = fs->fat_sector + fs->bpb.numFATs * fs->bpb.fatSz32;
    fs->rootDir_sector = fs->data_sector + fs->bpb.secPerClus * (fs->bpb.rootClus - 2);

    // Read fsinfo sector
    arm64_dcache_wbinv_range((vm_offset_t) fs->buf_va, 4096);
    err = long_sdhc_read(ds, fs->fsinfo_sector, get_phys_addr(fs->buf_cap));
    ON_ERR_RETURN(err);

    memcpy(&fs->fsi, fs->buf_va, sizeof(struct fs_info));
    assert(0x41615252 == fs->fsi.leadSig && "Error: Not an fsinfo sector");

    return err;
}

static struct fatfs_handle *handle_open(struct fatfs_mount *mount, struct fatfs_dirent *d, const char *path)
{
    // Malloc a handle pointer
    struct fatfs_handle *h = calloc(1, sizeof(*h));
    if (h == NULL) {
        debug_printf("Error: MALLOC FAIL");
        HERE;
        return NULL;
    }

    // Set handle to init vars
    h->common.mount = mount;
    h->isdir = d->is_dir;
    h->path = strdup(path);
    h->dirent = d;
    h->file_pos = 0;
    h->dir_pos = 0;

    return h;
}

static inline void handle_close(struct fatfs_handle *h)
{
    // Free all pointers in handle and handle itself
    free(h->path);
    free(h);
}

static errval_t find_dirent(struct fatfs_mount *mount, struct fatfs_dirent *root, const char *name,
                            struct fatfs_dirent **ret_de)
{
    errval_t err;
    if (!root->is_dir) {
        return FS_ERR_NOTDIR;
    }
    struct fatfs_dirent *d = root;
    struct fatfs_dirent *nd = calloc(1, sizeof(*nd));

    uint32_t current_cluster = d->content_cluster;
    uint32_t start_sector;
    //debug_printf(">> find dirent: |%s| in |%s| with cc |%d|\n", name, root->name, current_cluster);
    // Read linked clusters until entry is found of empty region is reached
    while((current_cluster & 0x0fffffff) != 0x0ffffff8 && (current_cluster & 0x0fffffff) != 0x0fffffff) {
        start_sector = mount->fs->data_sector + (current_cluster - 2) * mount->fs->bpb.secPerClus;
        //debug_printf(">> base sector: |%d|\n", start_sector);
        // Iterate through all sectors in cluster
        for (int i = 0; i < mount->fs->bpb.secPerClus; i++) {
            uint32_t current_sector = start_sector + i;
            //debug_printf(">> current sector: |%d|\n", current_sector);

            // Read sector
            err = long_sdhc_read(mount->ds, (int) current_sector, get_phys_addr(mount->fs->buf_cap));
            ON_ERR_RETURN(err);

            uint8_t *current = (uint8_t *) mount->fs->buf_va;

            // Iterate through all dir entrys in sector
            for(int j = 0; j < mount->fs->bpb.bytsPerSec; j += sizeof(struct fatfs_short_dirent)) {
                uint8_t *new_ptr = current + j;

                //debug_printf(">> current iter: |%d|\n", j);
                //debug_printf(">> current entry name: |%11s|\n", new_ptr);
                //debug_printf(">> current entry 1: |%d|\n", new_ptr[0]);
                //debug_printf(">> current entry 11: |%d|\n", new_ptr[11]);

                if (new_ptr[0] == 0 || new_ptr[11] == 0) {
                    // Reached empty region, file not found, terminate
                    // debug_printf(">> File not found\n");
                    return FS_ERR_NOTFOUND;
                } else if (new_ptr[0] == 0xE5) {
                    // Invalidated File, continue
                } else if ((new_ptr[11] & ATTR_LONG_NAME_MASK) == ATTR_LONG_NAME) {
                    // Reached long direntry
                    //debug_printf(">>> LONG NAME %x, NYI\n", ATTR_LONG_NAME);

                    struct fatfs_long_dirent dir;
                    memcpy(&dir, new_ptr, sizeof(struct fatfs_short_dirent));
                } else {  // Reached short direntry --> entry found
                    struct fatfs_short_dirent dir;
                    memcpy(&dir, new_ptr, sizeof(struct fatfs_short_dirent));

                    char *dirname = dir.name;

                    // Compare name (11B)
                    if (memcmp(name, dirname, 11) == 0){
                        // Entry found -> return
                        nd->name = dir.name;
                        nd->size = dir.fileSize;
                        nd->parent = root;
                        nd->cluster = current_cluster;
                        nd->sector = current_sector;
                        nd->sector_offset = j;
                        nd->is_dir = (dir.attr == ATTR_DIRECTORY);
                        nd->content_cluster = (((uint32_t) dir.fstClusHi) << 16) + dir.fstClusLow;
                        *ret_de = nd;
                        return SYS_ERR_OK;
                    }
                }
            }
        }

        // Check FAT table for next cluster
        err = get_next_fat_entry(mount, current_cluster, &current_cluster);
        ON_ERR_RETURN(err);
    }

    return FS_ERR_NOTFOUND;
}

static errval_t resolve_path(struct fatfs_mount *mount, const char *path,
                             struct fatfs_handle **ret_fh)
{
    errval_t err;
    struct fatfs_dirent *root = mount->root;

    if(path[0] == '\0') {
        // TODO: Test if a bug here
        return FS_ERR_NOTDIR;
    }

    // Skip leading /
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

        // Translate to fat32 namestyle
        char fat32name[11];

        // Special cases for link folder dot and dotdot (. , ..)
        if (strcmp(pathbuf, ".\0") == 0) {
            fat32name[0] = '.';
            for (int p = 1; p < 11; p++) {
                fat32name[p] = '0';
            }
        } else if (strcmp(pathbuf, "..\0") == 0) {
            fat32name[0] = '.';
            fat32name[1] = '.';
            for (int p = 2; p < 11; p++) {
                fat32name[p] = '0';
            }
        } else {
            pathname_to_fat32name(pathbuf, fat32name);
        }

        // Set rootname, else get parent
        if (memcmp(fat32name, root->name, 11) == 0) {
            //debug_printf(">>> GET TO HERE?\n");
            next_dirent = root;
        } else {
            err = find_dirent(mount, root, fat32name, &next_dirent);
            if (err_is_fail(err)) {
                //debug_printf(">> NO FOUND: |%s| in |%s|\n", fat32name, root->name);
                debug_printf("Error: Directory/File not found\n");
                return err;
            }
        }

        // Check if next directory to read is actually a directory
        if (!next_dirent->is_dir && nextsep != NULL) {
            return FS_ERR_NOTDIR;
        }

        // Set root to the next directory
        root = next_dirent;
        if (nextsep == NULL) {
            break;
        }

        pos += nextlen + 1;
    }

    //  Create handle
    if (ret_fh) {
        struct fatfs_handle *fh = handle_open(mount, root, path);
        if (fh == NULL) {
            return LIB_ERR_MALLOC_FAIL;
        }

        *ret_fh = fh;
    }

    return SYS_ERR_OK;
}

errval_t fatfs_open(void *st, const char *path, fatfs_handle_t *rethandle)
{
    errval_t err;
    struct fatfs_mount *mount = st;
    struct fatfs_handle *handle;

    // Open just means return a handle
    err = resolve_path(mount, path, &handle);
    if (err_is_fail(err)) {
        return err;
    }

    // If the returned entry is not a file, error. This is not fatfs_opendir!
    if (handle->isdir) {
        handle_close(handle);
        return FS_ERR_NOTFILE;
    }

    handle->file_pos = 0;
    *rethandle = handle;
    return SYS_ERR_OK;
}

errval_t fatfs_close(void *st, fatfs_handle_t inhandle)
{
    struct fatfs_handle *handle = inhandle;

    // Again, this is not fatfs_closedir!
    if (handle->isdir) {
        return FS_ERR_NOTFILE;
    }

    handle_close(handle);
    return SYS_ERR_OK;
}

static struct fatfs_dirent *dirent_create(const char *name, bool is_dir)
{
    // Allocate entry space
    struct fatfs_dirent *d = calloc(1, sizeof(*d));
    if (d == NULL) {
        return NULL;
    }

    // Set initial values
    d->is_dir = is_dir;
    d->name = strdup(name);
    d->size = 0;

    return d;
}

static errval_t dirent_insert(struct fatfs_mount *mount, struct fatfs_dirent *parent, struct fatfs_dirent *entry)
{
    errval_t err;
    assert(parent);
    assert(parent->is_dir);

    entry->parent = parent;
    bool exit = false;

    // Insert/write into folder-cluster/-sector on sdcard
    uint32_t current_cluster = parent->content_cluster;
    uint32_t start_sector;
    while(current_cluster != 0xffffff8) {
        start_sector = mount->fs->data_sector + (current_cluster - 2) * mount->fs->bpb.secPerClus;

        // Iterate through all sectors in cluster
        for (int i = 0; i < mount->fs->bpb.secPerClus; i++) {
            uint32_t current_sector = start_sector + i;

            // Read sector from sdcard
            err = long_sdhc_read(mount->ds, (int) current_sector, get_phys_addr(mount->fs->buf_cap));
            ON_ERR_RETURN(err);

            uint8_t *current = (uint8_t *) mount->fs->buf_va;

            // Iterate through all dirents in sector
            for(int j = 0; j < mount->fs->bpb.bytsPerSec; j += sizeof(struct fatfs_short_dirent)) {
                uint8_t *new_ptr = current + j;

                // Find evicted entry or new one
                if (new_ptr[0] == 0 || new_ptr[0] == 0xE5 || new_ptr[11] == 0) {
                    //debug_printf(">>> Successful write to: %d\n", j);
                    // Fill short_dir struct with infos and write to sdcard at newptr
                    struct fatfs_short_dirent dir;
                    memcpy(dir.name, entry->name, 11);

                    // Handle directory case
                    if (entry->is_dir) {
                        uint32_t content_cluster = 0;

                        // Handle linkdir case (dot, dotdot)
                        if (memcmp(".          ", entry->name, 11) != 0 && memcmp("..         ", entry->name, 11) != 0) {
                            // A real folder, create FAT entry
                            err = get_free_fat_entry(mount, &content_cluster);
                            ON_ERR_RETURN(err);

                        } else if (memcmp(".          ", entry->name, 11) == 0) {
                            content_cluster = parent->content_cluster;
                        } else {
                            content_cluster = parent->cluster;
                        }

                        dir.attr = ATTR_DIRECTORY;
                        entry->content_cluster = content_cluster;
                    } else {
                        dir.attr = ATTR_ARCHIVE;
                        entry->content_cluster = 0;
                    }
                    entry->cluster = current_cluster;
                    entry->sector = current_sector;
                    entry->sector_offset = j;

                    dir.ntRes = 0;
                    dir.crtTimeTenth = (uint8_t) (systime_to_us(systime_now())/1000);
                    dir.fstClusLow = (uint16_t) (entry->content_cluster & 0x0000FFFF);
                    dir.wrtTime = 0x514A;    // 10:10:20, not found function for time in bfish
                    dir.wrtDate = 0xEAA9;    // 29-05-2021, not found function for date in bfish
                    dir.fstClusHi = (uint16_t) ((entry->content_cluster >> 16) & 0x0000FFFF);
                    dir.fileSize = 0;

                    // Write new dir entry back to sdcard
                    err = long_sdhc_read(mount->ds, (int) entry->sector, get_phys_addr(mount->fs->buf_cap));
                    ON_ERR_RETURN(err);
                    //debug_printf(">>> WTF: |%d|\n", entry->sector_offset);
                    memcpy(mount->fs->buf_va + entry->sector_offset, &dir, sizeof(struct fatfs_short_dirent));

                    err = long_sdhc_write(mount->ds, (int) entry->sector, get_phys_addr(mount->fs->buf_cap));
                    ON_ERR_RETURN(err);
                    //debug_printf(">>> WTF: |%d|\n", entry->sector);

                    //err = long_sdhc_read(mount->ds, (int) entry->sector, get_phys_addr(mount->fs->buf_cap));
                    //ON_ERR_RETURN(err);
                    //struct fatfs_short_dirent *diro = (struct fatfs_short_dirent *)(mount->fs->buf_va + entry->sector_offset);
                    //debug_printf(">>> ROLOLO |%s|\n", diro->name);

                    exit = true;
                    break;
                }
            }
            if(exit){
                break;
            }
        }
        if(exit){
            break;
        }

        // Check Fat entry for more cluster
        uint32_t old_cluster = current_cluster;
        err = get_next_fat_entry(mount, current_cluster, &current_cluster);
        ON_ERR_RETURN(err);

        // Check if cluster is full
        if (current_cluster == -1){
            err = get_free_fat_entry(mount, &current_cluster);
            ON_ERR_RETURN(err);
            err = insert_new_fat_link(mount, old_cluster, current_cluster);
            ON_ERR_RETURN(err);
        }
    }
    return SYS_ERR_OK;
}

errval_t fatfs_create(void *st, const char *path, fatfs_handle_t *rethandle)
{
    //debug_printf(">>>> path: |%s|\n", path);
    errval_t err;
    struct fatfs_mount *mount = st;

    // Check if entry with that name already exists
    err = resolve_path(mount, path, NULL);
    if (err_is_ok(err)) {
        return FS_ERR_EXISTS;
    }

    struct fatfs_handle *parent = NULL;
    const char *childname;

    // Find parent directory
    char *lastsep = strrchr(path, FS_PATH_SEP);
    if (lastsep != NULL) {
        childname = lastsep + 1;

        size_t pathlen = lastsep - path;
        char pathbuf[pathlen + 1];
        memcpy(pathbuf, path, pathlen);
        pathbuf[pathlen] = '\0';

        // resolve parent directory
        err = resolve_path(mount, pathbuf, &parent);
        if (err_is_fail(err)) {
            return err;
        } else if (!parent->isdir) {
            return FS_ERR_NOTDIR; // parent is not a directory
        }
    } else {
        childname = path;
    }

    // Translate to fat32 namestyle
    char fat32name[11];
    pathname_to_fat32name(childname, fat32name);
    childname = fat32name;

    // Create direntry
    struct fatfs_dirent *dirent = dirent_create(childname, false);
    if (dirent == NULL) {
        return LIB_ERR_MALLOC_FAIL;
    }

    // Insert new entry in parent or root
    //debug_printf(">>> CHILDNAME: |%s|\n", dirent->name);
    if (parent) {
        //debug_printf(">>> SHOULD GET HERE name: |%s|\n", parent->dirent->name);
        //debug_printf(">>> SHOULD GET HERE clus: |%d|\n", parent->dirent->content_cluster);
        dirent_insert(mount, parent->dirent, dirent);
        handle_close(parent);
    } else {
        dirent_insert(mount, mount->root, dirent);
    }

    // Return a handle
    if (rethandle) {
        struct fatfs_handle *fh = handle_open(mount, dirent, path);
        if (fh  == NULL) {
            return LIB_ERR_MALLOC_FAIL;
        }
        fh->path = strdup(path);
        *rethandle = fh;
    }

    return SYS_ERR_OK;
}

errval_t fatfs_read(void *st, fatfs_handle_t handle, void *buffer, uint32_t bytes, size_t *bytes_read)
{
    errval_t err;
    struct fatfs_handle *h = handle;
    struct fatfs_mount *mount = st;

    // Check if that what we want to read is actually a file
    if (h->isdir) {
        return FS_ERR_NOTFILE;
    }

    assert(h->file_pos >= 0);

    // Check what we can read and adjust "bytes"
    if (h->dirent->content_cluster == 0 || h->dirent->content_cluster == -1) {
        bytes = 0;
    } else if (h->dirent->size < h->file_pos) {
        bytes = 0;
    } else if (h->dirent->size < h->file_pos + bytes) {
        bytes = h->dirent->size - h->file_pos;
        assert(h->file_pos + bytes == h->dirent->size);
    }

    // Can we read actually something?
    if (bytes == 0) {
        *bytes_read = bytes;
        debug_printf(">> EOF\n");
        return SYS_ERR_OK;
    }

    // Get cluster we want to read from
    uint32_t cluster_offset = h->file_pos/(mount->fs->bpb.bytsPerSec * mount->fs->bpb.secPerClus);
    uint32_t current_cluster = h->dirent->content_cluster;
    for (int i = 0; i < cluster_offset; i++) {
        // Read FAT
        get_next_fat_entry(mount, current_cluster, &current_cluster);

        if (current_cluster == -1) {
            debug_printf(">> Error: This should not happen\n");
        }
    }

    // Load sector from sdhc
    uint32_t sector = (int)(mount->fs->data_sector + (current_cluster - 2) * mount->fs->bpb.secPerClus + (h->file_pos / mount->fs->bpb.bytsPerSec));

    err = long_sdhc_read(mount->ds, (int) sector, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);

    // Adjust read length for simplicity
    bytes = MIN(mount->fs->bpb.bytsPerSec - h->file_pos%mount->fs->bpb.bytsPerSec, bytes);

    // Copy read bytes into buffer
    memcpy(buffer, mount->fs->buf_va + (h->file_pos%(mount->fs->bpb.bytsPerSec)), bytes);

    // Adjust index
    h->file_pos += bytes;
    *bytes_read = bytes;

    return SYS_ERR_OK;
}

errval_t fatfs_tell(void *st, fatfs_handle_t handle, size_t *pos)
{
    // Return position of the file pointer. Directory pointer is always 0
    struct fatfs_handle *h = handle;
    if (h->isdir) {
        *pos = 0;
    } else {
        *pos = h->file_pos;
    }
    return SYS_ERR_OK;
}

errval_t fatfs_stat(void *st, fatfs_handle_t inhandle, struct fs_fileinfo *info)
{
    // Return some infos (directory?, size?)
    struct fatfs_handle *h = inhandle;

    assert(info != NULL);
    info->type = h->isdir ? FS_DIRECTORY : FS_FILE;
    info->size = h->dirent->size;

    return SYS_ERR_OK;
}

errval_t fatfs_seek(void *st, fatfs_handle_t handle, enum fs_seekpos whence,
                    off_t offset)
{
    // Adjust file pointer position without reading anything
    errval_t err;
    struct fatfs_handle *h = handle;
    struct fs_fileinfo info;

    switch (whence) {
    case FS_SEEK_SET:
        assert(offset >= 0);
        if (h->isdir) {
            assert(!"NYI && not for directorys");
        } else {
            h->file_pos = offset;
        }
        break;

    case FS_SEEK_CUR:
        if (h->isdir) {
            assert(!"NYI && not for directorys");
        } else {
            assert(offset >= 0 || -offset <= h->file_pos);
            h->file_pos += offset;
        }

        break;

    case FS_SEEK_END:
        if (h->isdir) {
            assert(!"NYI && directorys have no pointer here");
        } else {
            err = fatfs_stat(st, handle, &info);
            if (err_is_fail(err)) {
                return err;
            }
            assert(offset >= 0 || -offset <= info.size);
            h->file_pos = (off_t) (info.size + offset);
        }
        break;

    default:
        USER_PANIC("invalid whence argument to fatfs seek");
    }

    return SYS_ERR_OK;
}

static errval_t fatfs_mkdir_dots(struct fatfs_mount *mount, struct fatfs_dirent *parent, char *folder_name)
{
    // MKDIR function specific for link folder (dot, dotdot)
    errval_t err;

    struct fatfs_dirent *dirent = dirent_create(folder_name, true);
    if (dirent == NULL) {
        return LIB_ERR_MALLOC_FAIL;
    }

    err = dirent_insert(mount, parent, dirent);
    ON_ERR_RETURN(err);
    return SYS_ERR_OK;
}


errval_t fatfs_mkdir(void *st, const char *path)
{
    errval_t err;
    struct fatfs_mount *mount = st;

    // Check if directory exists already
    err = resolve_path(mount, path, NULL);
    if (err_is_ok(err)) {
        return FS_ERR_EXISTS;
    }

    struct fatfs_handle *parent = NULL;
    const char *childname;

    // Find parent directory
    char *lastsep = strrchr(path, FS_PATH_SEP);
    if (lastsep != NULL) {
        childname = lastsep + 1;
        size_t pathlen = lastsep - path;
        char pathbuf[pathlen + 1];
        memcpy(pathbuf, path, pathlen);
        pathbuf[pathlen] = '\0';

        // Resolve parent directory
        err = resolve_path(mount, pathbuf, &parent);
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

    // Translate to fat32 namestyle
    char fat32name[11];
    pathname_to_fat32name(childname, fat32name);
    childname = fat32name;

    // Create new directory entry
    struct fatfs_dirent *dirent = dirent_create(childname, true);
    if (dirent == NULL) {
        return LIB_ERR_MALLOC_FAIL;
    }

    // Insert new directory entry in parent or root
    if (parent) {
        dirent_insert(mount, parent->dirent, dirent);
        handle_close(parent);
    } else {
        dirent_insert(mount, mount->root, dirent);
    }

    // Create link directorys "to itself" (dot) and "parent" (dotdot)
    fatfs_mkdir_dots(mount, dirent, ".          ");
    fatfs_mkdir_dots(mount, dirent, "..         ");
    return SYS_ERR_OK;
}

errval_t fatfs_opendir(void *st, const char *path, fatfs_handle_t *rethandle)
{
    errval_t err;
    struct fatfs_mount *mount = st;
    struct fatfs_handle *handle;

    // Check if entry exists
    err = resolve_path(mount, path, &handle);
    if (err_is_fail(err)) {
        return err;
    }

    // Check if it is actually a directory
    if (!handle->isdir) {
        handle_close(handle);
        return FS_ERR_NOTDIR;
    }

    *rethandle = handle;
    return SYS_ERR_OK;
}

errval_t fatfs_dir_read_next(void *st, fatfs_handle_t inhandle, char **retname,
                             struct fs_fileinfo *info)
{
    errval_t err;
    struct fatfs_mount *mount = st;
    struct fatfs_handle *h = inhandle;

    // Check if entry is a directory
    if (!h->isdir) {
        return FS_ERR_NOTDIR;
    }

    // Get cluster from FAT
    uint32_t cluster_offset = h->dir_pos / (mount->fs->bpb.bytsPerSec * mount->fs->bpb.secPerClus);
    uint32_t current_cluster = h->dirent->content_cluster;
    for (int i = 0; i < cluster_offset; i++) {
        err = get_next_fat_entry(mount, current_cluster, &current_cluster);
        ON_ERR_RETURN(err);

        if (current_cluster == -1) {
            debug_printf(">> Error: This should not happen\n");
        }
    }

    // Read next folder entry from sector
    uint32_t sector = (int)(mount->fs->data_sector + (current_cluster - 2) * mount->fs->bpb.secPerClus + (h->dir_pos / mount->fs->bpb.bytsPerSec));

    err = long_sdhc_read(mount->ds, (int) sector, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);

    struct fatfs_short_dirent fsd;
    memcpy(&fsd, mount->fs->buf_va + (h->dir_pos % mount->fs->bpb.bytsPerSec), sizeof(struct fatfs_short_dirent));

    // Check if entry is valid
    if (fsd.attr == 0x0) {
        return FS_ERR_INDEX_BOUNDS;
    }

    // Set return values
    if (retname != NULL) {
        *retname = strdup(fsd.name);
    }

    if (info != NULL) {
        info->type = (fsd.attr == ATTR_DIRECTORY) ? FS_DIRECTORY : FS_FILE;
        info->size = fsd.fileSize;
    }

    // Increase position by the size of a short entry (always 32B)
    h->dir_pos += sizeof(struct fatfs_short_dirent);

    return SYS_ERR_OK;
}

errval_t fatfs_closedir(void *st, fatfs_handle_t dhandle)
{
    struct fatfs_handle *handle = dhandle;

    // Check if the folder is not a file
    if (!handle->isdir) {
        return FS_ERR_NOTDIR;
    }

    // Free all pointer in the handle struct and the handle itself
    free(handle->path);
    free(handle);

    return SYS_ERR_OK;
}

errval_t fatfs_write(void *st, fatfs_handle_t handle, const void *buffer,
                     size_t bytes, size_t *bytes_written)
{
    //debug_printf(">> ENTRY WRITE\n");
    errval_t err;
    struct fatfs_mount *mount = st;
    struct fatfs_handle *h = handle;

    assert(h->file_pos >= 0);

    // Check if it is a file
    if (h->isdir) {
        return FS_ERR_NOTFILE;
    }

    // For simplicity only writes to the end possible (at least for the start)
    h->file_pos = (off_t) h->dirent->size;
    size_t offset = h->file_pos;

    size_t sector_offset = offset / mount->fs->bpb.bytsPerSec;
    size_t cluster_offset = sector_offset / mount->fs->bpb.secPerClus;
    uint32_t current_cluster = h->dirent->content_cluster;

    size_t bytes_to_write = MIN(bytes, mount->fs->bpb.bytsPerSec - (offset % mount->fs->bpb.bytsPerSec));
    // Get current cluster
    if (current_cluster != 0) {
        // Content cluster available
        // Check if the one we need to access does exist
        uint32_t old_cluster = current_cluster;
        for (int f = 0; f < cluster_offset; f++) {
            old_cluster = current_cluster;
            err = get_next_fat_entry(mount, current_cluster, &current_cluster);
            ON_ERR_RETURN(err);
        }

        // If the cluster we need to access does not exist create one
        if (current_cluster == -1) {
            err = get_free_fat_entry(mount, &current_cluster);
            ON_ERR_RETURN(err);

            err = insert_new_fat_link(mount, old_cluster, current_cluster);
            ON_ERR_RETURN(err);
        }
    } else {
        //debug_printf(">>> Should come here\n");
        // There is no cluster available, so create one
        err = get_free_fat_entry(mount, &current_cluster);
        ON_ERR_RETURN(err);

        // Write content cluster origin into file entry, important, sector includes data offset
        err = long_sdhc_read(mount->ds, (int) h->dirent->sector, get_phys_addr(mount->fs->buf_cap));
        ON_ERR_RETURN(err);

        struct fatfs_short_dirent *dir = ((struct fatfs_short_dirent *) ((uint8_t *) mount->fs->buf_va + h->dirent->sector_offset));
        //debug_printf(">>> cluster: %d\n", current_cluster);
        //debug_printf(">>> sector offset: %d\n", h->dirent->sector_offset);
        dir->fstClusLow = (uint16_t) (current_cluster & 0x0000FFFF);
        dir->fstClusHi = (uint16_t) ((current_cluster >> 16) & 0x0000FFFF);

        //debug_printf(">>> clusterlow: %d\n", dir->fstClusLow);
        //debug_printf(">>> clusterhi: %d\n", dir->fstClusHi);
        //debug_printf(">>> sector: %d\n", h->dirent->sector);
        err = long_sdhc_write(mount->ds, (int) h->dirent->sector, get_phys_addr(mount->fs->buf_cap));
        ON_ERR_RETURN(err);

        // Update handler directory entry
        h->dirent->content_cluster = current_cluster;
        current_cluster = current_cluster;
    }
    //debug_printf(">>> cluster: %d\n", current_cluster);
    // Mount the cluster we want to write into / don't forget the sector offset to get the right sector
    size_t sector = mount->fs->data_sector + ((current_cluster - 2) * mount->fs->bpb.secPerClus) + (sector_offset % mount->fs->bpb.secPerClus);
    err = long_sdhc_read(mount->ds, (int) sector, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);
    //debug_printf(">>> write off: %zu\n", offset);
    //debug_printf(">>> write secoff: %zu\n", sector_offset);
    memcpy(mount->fs->buf_va + (offset % mount->fs->bpb.bytsPerSec), buffer, bytes_to_write);
    //debug_printf(">>> to write: |%s|\n", (char *) buffer);
    err = long_sdhc_write(mount->ds, (int) sector, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);

    err = long_sdhc_read(mount->ds, (int) sector, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);
    //debug_printf(">>> to write: |%s|\n", (char *) mount->fs->buf_va);

    // Set return values
    if (bytes_written) {
        *bytes_written = bytes_to_write;
    }

    // Adjust handle index variables
    h->file_pos += (off_t) bytes_to_write;
    h->dirent->size += bytes_to_write;

    // Write updated file size into file entry
    err = long_sdhc_read(mount->ds, (int) h->dirent->sector, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);

    struct fatfs_short_dirent *dir = ((struct fatfs_short_dirent *) ((uint8_t *) mount->fs->buf_va + h->dirent->sector_offset));
    //debug_printf(">>>>>> OIOIOIsi %u\n", (uint32_t) h->dirent->size);
    //debug_printf(">>>>>> OIOIOIsec %u\n", (uint32_t) h->dirent->sector_offset);
    //debug_printf(">>>>>> OIOIOInam |%11s|\n", dir->name);

    dir->fileSize = (uint32_t) h->dirent->size;

    err = long_sdhc_write(mount->ds, (int) h->dirent->sector, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);

    //debug_printf(">> WEIRD\n");
    return SYS_ERR_OK;
}

__unused
errval_t fatfs_truncate(void *st, fatfs_handle_t handle, size_t bytes)
{
    errval_t err;
    struct fatfs_mount *mount = st;
    struct fatfs_handle *h = handle;

    // Check if it is a file
    if (h->isdir) {
        return FS_ERR_NOTFILE;
    }

    // Check if we can truncate number of bytes
    if (h->dirent->size <= bytes) {
        return FS_ERR_INDEX_BOUNDS;
    }

    // Change the size in file
    err = long_sdhc_read(mount->ds, (int) h->dirent->sector, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);

    struct fatfs_short_dirent *dir = ((struct fatfs_short_dirent *) ((uint8_t *) mount->fs->buf_va + h->dirent->sector_offset));
    dir->fileSize = (uint32_t) bytes;

    // If bytes is zero, delete hi and low in file --> delete assigned content cluster
    if (bytes == 0){
        dir->fstClusLow = (uint16_t) 0;
        dir->fstClusHi = (uint16_t) 0;
    }

    err = long_sdhc_write(mount->ds, (int) h->dirent->sector, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);

    // Remove clusters from FAT (Set to 0x0) (if cluster boundary is crossed)
    // Get cluster_offset which ends in bytes
    size_t sector_offset = bytes / mount->fs->bpb.bytsPerSec;
    size_t cluster_offset = sector_offset / mount->fs->bpb.secPerClus;
    uint32_t current_cluster = h->dirent->content_cluster;
    size_t out_cluster = current_cluster;

    // FAT tablewalk to remove entries
    for (int c = 0; ((current_cluster & 0x0fffffff) != 0x0fffffff) && ((current_cluster & 0x0fffffff) != 0x0ffffff8); c++) {
        //debug_printf(">> SO LONG\n");
        uint32_t old_cluster = current_cluster;
        get_next_fat_entry(mount, current_cluster, &current_cluster);

        // Apply conditional changes
        if (c > cluster_offset) {
            insert_new_fat_link(mount, old_cluster, 0);
        } else if (c == cluster_offset) {
            if (bytes == 0) {
                insert_new_fat_link(mount, old_cluster, 0);;
            } else {
                insert_new_fat_link(mount, old_cluster, 0x0fffffff);
            }
        } else {
            out_cluster = current_cluster;
        }
    }

    // Change size in handle dir entry
    h->dirent->size = bytes;
    h->dirent->content_cluster = out_cluster;
    h->file_pos = MIN(h->file_pos, bytes);

    return SYS_ERR_OK;
}

errval_t fatfs_remove(void *st, const char *path)
{
    //debug_printf(">> REACHED REMOVE\n");
    errval_t err;
    struct fatfs_mount *mount = st;
    struct fatfs_handle *h;

    // Get handle
    err = resolve_path(mount, path, &h);
    if (err_is_fail(err)){
        // file does not exist
        //debug_printf(">> FILE NOT FOUND\n");
        return FS_ERR_NOTFOUND;
    }

    // Truncate to 0
    //debug_printf(">> REACHED TRUNCATE START\n");
    err = fatfs_truncate(st, h, 0);
    //debug_printf(">> REACHED TRUNCATE ERR\n");
    ON_ERR_RETURN(err);
    //debug_printf(">> REACHED TRUNCATE\n");
    // Set first byte in file entry to 0xE5 and attr to 0
    err = long_sdhc_read(mount->ds, (int) h->dirent->sector, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);

    uint8_t *dir = ((uint8_t *) mount->fs->buf_va) + h->dirent->sector_offset;
    dir[0] = 0xE5;
    dir[11] = 0;

    err = long_sdhc_write(mount->ds, (int) h->dirent->sector, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);
    err = long_sdhc_read(mount->ds, (int) h->dirent->sector, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);
    dir = ((uint8_t *) mount->fs->buf_va) + h->dirent->sector_offset;
    //debug_printf(">> REACHED 0x%x\n", dir[0]);
    handle_close(h);
    return SYS_ERR_OK;
}

errval_t fatfs_rmdir(void *st, const char *path)
{
    errval_t err;
    struct fatfs_mount *mount = st;
    struct fatfs_handle *h;

    // Get handle
    err = resolve_path(mount, path, &h);
    if (err_is_fail(err)){
        // Folder does not exist
        return FS_ERR_NOTFOUND;
    }

    // Check if folder is empty (only contains . and ..)
    uint8_t start_byte = 1;
    uint32_t current_cluster = h->dirent->cluster;
    size_t start_offset = 2 * sizeof(struct fatfs_short_dirent); // To skip . and ..
    while (start_byte != 0 && ((current_cluster & 0x0fffffff) != 0x0fffffff) && ((current_cluster & 0x0fffffff) != 0x0ffffff8)) {
        int sector = (int) (mount->fs->data_sector + (current_cluster - 2) * mount->fs->bpb.secPerClus);
        for (int i = 0; (i < mount->fs->bpb.secPerClus) && (start_byte != 0); i++) {
            err = long_sdhc_read(mount->ds, sector + i, get_phys_addr(mount->fs->buf_cap));
            ON_ERR_RETURN(err);

            for (uint8_t *addr = mount->fs->buf_va + start_offset; (addr - (uint8_t *) mount->fs->buf_va) < mount->fs->bpb.bytsPerSec; addr += sizeof(struct fatfs_short_dirent)) {
                if ((addr[11] != 0) && (addr[0] != 0xE5) && (addr[0] != 0x00)) {
                    handle_close(h);
                    return FS_ERR_NOTEMPTY;
                }
                if ((addr[0] == 0) && (addr[11] == 0)) {
                    start_byte = 0;
                    break;
                }
            }
            start_offset = 0;
        }

        // Update current_cluster / Walk FAT table
        err = get_next_fat_entry(mount, current_cluster, &current_cluster);
        ON_ERR_RETURN(err);
    }

    // . and .. dont have to be deleted, as cluster will be set to 0 on new folder allocation
    // Remove content cluster from fat (set to 0x0)
    current_cluster = h->dirent->content_cluster;
    uint32_t parent = current_cluster;
    while (((parent & 0x0fffffff) != 0x0fffffff) && ((parent & 0x0fffffff) != 0x0ffffff8)) {
        uint32_t child;
        err = get_next_fat_entry(mount, parent, &child);
        ON_ERR_RETURN(err);

        err = insert_new_fat_link(mount, parent, 0);
        ON_ERR_RETURN(err);

        parent = child;
    }

    // Set first byte in dir entry to 0xE5 and attr to 0
    err = long_sdhc_read(mount->ds, (int) h->dirent->sector, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);

    uint8_t *dir = ((uint8_t *) mount->fs->buf_va) + h->dirent->sector_offset;
    dir[0] = 0xE5;
    dir[11] = 0;

    err = long_sdhc_write(mount->ds, (int) h->dirent->sector, get_phys_addr(mount->fs->buf_cap));
    ON_ERR_RETURN(err);

    handle_close(h);
    return SYS_ERR_OK;
}

errval_t fatfs_mount(const char *path, fatfs_mount_t *retst)
{
    // Setup channel and connect ot service
    // TODO: setup channel to init for multiboot files
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
    err = initialize_fat32_partition(ds, fs);
    if (err_is_fail(err)){
        free(ds);
        free(fs);
        return OCT_ERR_ENGINE_FAIL;
    }

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
    char fat32name[11];
    pathname_to_fat32name(path+1, fat32name);
    fatfs_root->size = 0;
    fatfs_root->is_dir = true;
    fatfs_root->name = strdup(fat32name);
    fatfs_root->parent = NULL;
    fatfs_root->cluster = fs->bpb.rootClus;
    fatfs_root->sector_offset = 0;
    fatfs_root->sector = fs->rootDir_sector;
    fatfs_root->size = 0;
    fatfs_root->content_cluster = fatfs_root->cluster;

    mount->root = fatfs_root;

    mount->fs = fs;
    mount->ds = ds;

    *retst = mount;

    //measurments(mount);
    return SYS_ERR_OK;



    debug_printf("==========TESTING==========\n");
    debug_printf(">>> Print ROOT: |%s|\n", mount->root->name);
    debug_printf(">>> RootDirSec: %d\n", fs->rootDir_sector);
    debug_printf(">>> RootClus: %d\n", fs->bpb.rootClus);
    uint8_t *current;
    for (int j = 0; j < fs->bpb.secPerClus/fs->bpb.secPerClus; j++) {
        err = long_sdhc_read(ds, fs->rootDir_sector + j, get_phys_addr(fs->buf_cap));
        ON_ERR_RETURN(err);
        current = fs->buf_va;
        for (int i = 0; i < 16/8; i++) {
            current += i * 32;
            if (((current[11] & ATTR_LONG_NAME_MASK) == ATTR_LONG_NAME)
                && (current[0] != 0xE5)) {
                struct fatfs_long_dirent dir;
                memcpy(&dir, current, sizeof(struct fatfs_short_dirent));
                debug_printf(">> ord:   %hhu\n", dir.ord);
                debug_printf(">> name1: %5ls\n", dir.name1);
                debug_printf(">> attr:  %hhu\n", dir.attr);
                debug_printf(">> type:  %hhu\n", dir.type);
                debug_printf(">> chsm:  %hhu\n", dir.chksum);
                debug_printf(">> name2: %6ls\n", dir.name2);
                debug_printf(">> lo:    %hu\n", dir.fstClusLo);
                debug_printf(">> name3: %2ls\n", dir.name3);
                debug_printf(">> <<\n");
            //} else if ((current[0] == 0xE5 || current[0] == 0) && current[11] == 0) {
                // Do nothing
            } else {

                struct fatfs_short_dirent dir;
                memcpy(&dir, current, sizeof(struct fatfs_short_dirent));
                debug_printf(">> name:  %11s\n", dir.name);
                debug_printf(">> attr:  0x%x\n", dir.attr);
                debug_printf(">> ntRes: 0x%x\n", dir.ntRes);
                debug_printf(">> cTime: %hhu\n", dir.crtTimeTenth);
                debug_printf(">> hi:    %hu\n", dir.fstClusHi);
                debug_printf(">> wTime: %hu\n", dir.wrtTime);
                debug_printf(">> wDate: %hu\n", dir.wrtDate);
                debug_printf(">> lo:    %hu\n", dir.fstClusLow);
                debug_printf(">> size:  %u\n", dir.fileSize);
                debug_printf(">> <<\n");
            }
        }
    }

    err = long_sdhc_read(ds, fs->data_sector + (3-2) * fs->bpb.secPerClus, get_phys_addr(fs->buf_cap));
    ON_ERR_RETURN(err);

    debug_printf(">>> Print FOLDER\n");
    current = fs->buf_va;
    for (int i = 0; i < 16; i++) {
        current += i*32;
        if (((current[11] & ATTR_LONG_NAME_MASK) == ATTR_LONG_NAME) && (current[0] != 0xE5)) {
            struct fatfs_long_dirent dir;
            memcpy(&dir, current, sizeof(struct fatfs_short_dirent));
            debug_printf(">> ord:   %hhu\n", dir.ord);
            debug_printf(">> name1: %5ls\n", dir.name1);
            debug_printf(">> attr:  0x%x\n", dir.attr);
            debug_printf(">> type:  0x%x\n", dir.type);
            debug_printf(">> chsm:  %hhu\n", dir.chksum);
            debug_printf(">> name2: %6ls\n", dir.name2);
            debug_printf(">> lo:    %hu\n", dir.fstClusLo);
            debug_printf(">> name3: %2ls\n", dir.name3);
            debug_printf(">> <<\n");
        } else if (current[0] == 0xE5 || current[11] == 0x0) {
            // Do nothing
        } else {
            struct fatfs_short_dirent dir;
            memcpy(&dir, current, sizeof(struct fatfs_short_dirent));
            debug_printf(">> name:  %11s\n", dir.name);
            debug_printf(">> attr:  0x%x\n", dir.attr);
            debug_printf(">> ntRes: 0x%x\n", dir.ntRes);
            debug_printf(">> cTime: %hhu\n", dir.crtTimeTenth);
            debug_printf(">> hi:    %hu\n", dir.fstClusHi);
            debug_printf(">> wTime: %hu\n", dir.wrtTime);
            debug_printf(">> wDate: %hu\n", dir.wrtDate);
            debug_printf(">> lo:    %hu\n", dir.fstClusLow);
            debug_printf(">> size:  %u\n", dir.fileSize);
            debug_printf(">> <<\n");
        }
    }

    err = long_sdhc_read(ds, fs->data_sector + (4-2) * fs->bpb.secPerClus, get_phys_addr(fs->buf_cap));
    ON_ERR_RETURN(err);

    debug_printf(">>> Print FILE SHORT\n");
    for (int i = 0; i < 1; i++) {
        debug_printf(">> Filesize: %lu\n", strlen((char *) fs->buf_va));
        debug_printf(">> Content:  %s\n", (uint8_t *) fs->buf_va);
        debug_printf(">> <<\n");
    }

    debug_printf(">>> Print FAT\n");
    err = long_sdhc_read(ds, fs->fat_sector, get_phys_addr(fs->buf_cap));
    ON_ERR_RETURN(err);

    uint32_t fatentry;
    for (int i = 0; i < 10; i++) {
        fatentry = *(uint32_t *) (fs->buf_va+i*sizeof(uint32_t));
        debug_printf(">> fatentry %d: 0x%x\n", i, fatentry);
    }
/*
    err = long_sdhc_read(ds, fs->data_sector + (7-2) * fs->bpb.secPerClus, get_phys_addr(fs->buf_cap));
    ON_ERR_RETURN(err);

    debug_printf(">>> Print FILE LONG\n");
    for (int i = 0; i < 1; i++) {
        debug_printf(">> Filesize: %lu\n", strlen((char *) fs->buf_va));
        debug_printf(">> Content:  %s\n", (uint8_t *) fs->buf_va);
        debug_printf(">> <<\n");
    }
*/
    return SYS_ERR_OK;
}

__unused
void measurments(struct fatfs_mount *mount)
{
    systime_t start, end;
    uint64_t diff;
    debug_printf(">> Sequential reads:\n");
    for(int i = 0; i < 100; i++) {
        start = systime_now();
        long_sdhc_read(mount->ds, i, get_phys_addr(mount->fs->buf_cap));
        end = systime_now();
        diff = systime_to_us(end - start)/1000;
        debug_printf(">> ms: %lu\n", diff);
    }

    debug_printf(">> Sequential writes:\n");
    for(int i = 0; i < 100; i++) {
        start = systime_now();
        long_sdhc_read(mount->ds, i, get_phys_addr(mount->fs->buf_cap));
        end = systime_now();
        diff = systime_to_us(end - start)/1000;
        debug_printf(">> ms: %lu\n", diff);
    }

    debug_printf(">> Sequential read/writes:\n");
    for(int i = 0; i < 50; i++) {
        start = systime_now();
        long_sdhc_read(mount->ds, i, get_phys_addr(mount->fs->buf_cap));
        long_sdhc_write(mount->ds, i, get_phys_addr(mount->fs->buf_cap));
        end = systime_now();
        diff = systime_to_us(end - start)/1000;
        debug_printf(">> ms: %lu\n", diff);
    }

}