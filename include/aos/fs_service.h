#define FS_SERVICE_NAME "/fs"
#include <aos/nameserver.h>

enum fs_service_messagetype {
    F_READ,
    F_WRITE,
    F_RM,
    D_READ,
    D_MKDIR,
    D_RM,
    SPAWN_ELF
};


struct fs_service_message {
    enum fs_service_messagetype type;
    size_t path_size;
    size_t data_size;
    char data[0];
} __attribute__((__packed__));

void init_fs(void);

void read_file(char *path, size_t size, char *ret);

void write_file(char *path, char *data);

void delete_file(char *path);

void read_dir(char *path, char **ret);

void create_dir(char *path);

void delete_dir(char *path);

void spawn_elf_file(char* path);
