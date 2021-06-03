#include <aos/nameserver.h>
#define FS_SERVICE_NAME "/fs"

enum fs_service_messagetype {
    F_READ=0,
    F_WRITE=1,
    F_RM=2,
    D_READ=3,
    D_MKDIR=4,
    D_RM=5,
    SPAWN_ELF=6
};


struct fs_service_message {
    enum fs_service_messagetype type;
    size_t path_size;
    size_t data_size;
    char data[0];
} __attribute__((__packed__));

void read_file(char *path, size_t size, char *ret);

void write_file(char *path, char *data);

void delete_file(char *path);

void read_dir(char *path, char **ret);

void create_dir(char *path);

void delete_dir(char *path);

void spawn_elf_file(char* path);
