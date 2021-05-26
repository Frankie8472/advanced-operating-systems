/*
 * Copyright (c) 2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr 6, CH-8092 Zurich. Attn: Systems Group.
 */

//#ifndef FS_RAMFS_H_
//#define FS_RAMFS_H_

#include <fs/fs.h>
#include <drivers/sdhc.h>

typedef void *fatfs_handle_t;
typedef void *fatfs_mount_t;

/**
 * \brief BPB of the fat32fs (90 Bytes)
 */
struct fatfs_bpb
{
    uint8_t jmpBoot[3];         ///< jump instr to boot code
    char OEMName[8];            ///< name of the fs (ambiguous, os specific)
    uint16_t bytsPerSec;        ///< count of bytes per sector: 512, 1024, 2048 or 4096
    uint8_t secPerClus;         ///< number of sectors per allocation unit 1, 2, 4, 8, 16, 32, 64 or 128
    uint16_t rsvdSecCnt;        ///< number of reserved sectors, always 1, never 0
    uint8_t numFATs;            ///< count of FAT data structures, always 2
    uint16_t rootEntCnt;        ///< zero - only for FAT12 and FAT16
    uint16_t totSec16;          ///< zero - only for FAT12 and FAT16
    uint8_t media;              ///< for removable media, 0xF0 is frequently used (0xF0, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, and 0xFF)
    uint16_t fatSz16;           ///< only for FAT16 --> must be 0
    uint16_t secPerTrk;         ///< sectors per track geometry value for interupt 0x13
    uint16_t numHeads;          ///< number of heads for interupt 0x13
    uint32_t hiddSec;           ///< count of hidden sectors preceding the partition that contains this FAT volume.  This field should always be zero on media that are not partitioned
    uint32_t totSec32;          ///< non-zero - count of sectors on the volume
    // offset 36
    uint32_t fatSz32;           ///< count of sectors occupied by ONE FAT
    uint16_t extFlags;          ///< some flags for mirroring
    uint16_t fsVer;             ///< non-zero - High byte major revision, low byte minor revision number
    uint32_t rootClus;          ///< cluster number of the first cluster on the root dir (usually 2)
    uint16_t fsInfo;            ///< sector number of fsinfo struct in the reserved area (usually 1)
    uint16_t bkBootSec;         ///< indicates the sector number in the reserved area of the volume of a copy of the boot record(usually 6)
    uint8_t reserved[12];       ///< reserved for future expansion --> must be 0
    uint8_t drvNum;             ///< int 0x13 drive number (os specific though)
    uint8_t reserved1;          ///< reserved (used by Windows NT). Code that formats FAT volumes --> must be 0
    uint8_t bootSig;            ///< extended boot signature (0x29)
    uint32_t volId;             ///< volume serial number (supports volume tracking on removable media)
    char volLab[11];            ///< volume label, matches the 11-byte colume label recorded in the root dir
    char filSysType[8];         ///< always set to the string ”FAT32 ”
} __attribute__((packed));
static_assert(sizeof(struct fatfs_bpb) == 90);

/**
 * \brief fsInfo struct (508 Bytes)
 */
struct fs_info
{
    uint32_t leadSig;           ///< value 0x41615252. This lead signature is used to validate that this is in fact an FSInfo sector
    uint8_t reserved1[480];     ///< zero - Bytes in this field must currently never be used
    uint32_t strucSig;          ///< value 0x61417272. Another signature that is more localized in the sector to the location of the fields that are used.
    uint32_t free_Count;        ///< last known free cluster count on the volume. If the value is 0xFFFFFFFF, then the free count is unknown and must be computed.
    uint32_t nxt_Free;          ///< cluster number at which the driver should start looking for free clusters
    uint8_t reserved2[12];      ///< Always set to the string ”FAT32 ”
} __attribute__((packed));
static_assert(sizeof(struct fs_info) == 508);

/**
 * \brief fat32 short dir entry (32 Bytes)
 */
struct fatfs_short_dirent
{
    char name[11];              ///< short name
    uint8_t attr;               ///< the upper two bits of the attribute byte are reserved and should always be set to 0 when a file is created and never modified or looked at after that.
    uint8_t ntRes;              ///< reserved for use by Windows NT. Set value to 0 when a file is created and never modify or look at it after that.
    uint8_t crtTimeTenth;       ///< millisecond stamp at file creation time.
    uint8_t unused[6];          ///< unused space
    uint16_t fstClusHi;         ///< high word of this entry's first cluster number
    uint16_t wrtTime;           ///< time of last write
    uint16_t wrtDate;           ///< date of last write
    uint16_t fstClusLow;        ///< low word of this entry's first cluster number
    uint32_t fileSize;          ///< file's size in bytes
} __attribute__((packed));
static_assert(sizeof(struct fatfs_short_dirent) == 32);

/**
 * \brief fat32 long dir entry (32 Bytes)
 */
struct fatfs_long_dirent
{
    uint8_t ord;                ///< order of this entry in the seq of long dir entries
    uint16_t name1[5];             ///< Characters 1-5 of the long-name sub-component in this dir entry
    uint8_t attr;               ///< the upper two bits of the attribute byte are reserved and should always be set to 0 when a file is created and never modified or looked at after that.
    uint8_t type;               ///< must be 0 - indicates a directory entry that is a sub-component of a long name.
    uint8_t chksum;             ///< checksum of name in the short dir entry ath the end of the long dir set
    uint16_t name2[6];             ///< characters 6-11 of the long-name sub-component in this dir entry
    uint16_t fstClusLo;         ///< must be 0 - artifact of the FAT "first cluster"
    uint16_t name3[2];              ///< characters 12-13 of the long-name sub-component in this dir entry
} __attribute__((packed));
static_assert(sizeof(struct fatfs_long_dirent) == 32);

struct fat32_fs
{
    struct capref buf_cap;
    void *buf_va;
    struct fatfs_bpb bpb;
    struct fs_info fsi;
    uint16_t bpb_sector;
    uint16_t fsinfo_sector;
    uint16_t fat_sector;
    uint16_t data_sector;
    uint16_t rootDir_sector;
};

struct fatfs_mount {
    struct fatfs_dirent *root;
    struct fat32_fs *fs;
    struct sdhc_s *ds;
};

errval_t fatfs_open(void *st, const char *path, fatfs_handle_t *rethandle);

errval_t fatfs_close(void *st, fatfs_handle_t inhandle);
errval_t fatfs_create(void *st, const char *path, fatfs_handle_t *rethandle);

errval_t fatfs_read(void *st, fatfs_handle_t handle, void *buffer, uint32_t bytes,
                    size_t *bytes_read);

errval_t fatfs_tell(void *st, fatfs_handle_t handle, size_t *pos);

errval_t fatfs_stat(void *st, fatfs_handle_t inhandle, struct fs_fileinfo *info);


/*


errval_t ramfs_remove(void *st, const char *path);


errval_t ramfs_write(void *st, ramfs_handle_t handle, const void *buffer,
                     size_t bytes, size_t *bytes_written);

errval_t ramfs_truncate(void *st, ramfs_handle_t handle, size_t bytes);



errval_t ramfs_seek(void *st, ramfs_handle_t handle, enum fs_seekpos whence,
                    off_t offset);


errval_t ramfs_opendir(void *st, const char *path, ramfs_handle_t *rethandle);

errval_t ramfs_dir_read_next(void *st, ramfs_handle_t inhandle, char **retname,
                             struct fs_fileinfo *info);

errval_t ramfs_closedir(void *st, ramfs_handle_t dhandle);

errval_t ramfs_mkdir(void *st, const char *path);

errval_t ramfs_rmdir(void *st, const char *path);
*/
errval_t fatfs_mount(const char *uri, fatfs_mount_t *retst);

