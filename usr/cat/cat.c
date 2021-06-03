/**
 * \file
 * \brief cat application
 */

#include <aos/aos.h>
#include <aos/aos_datachan.h>
#include <aos/dispatcher_rpc.h>
#include <stdio.h>
#include <aos/fs_service.h>

int main(int argc, char *argv[])
{
    if (argc == 1) {
        char buffer[1];
        size_t read;
        do {
            read = fread(buffer, 1, sizeof buffer, stdin);
            fwrite(buffer, 1, read, stdout);
            fflush(stdout);
        } while (read > 0);
        return 0;
    }
    else {
        char ret[1000];
        read_file(argv[1], sizeof ret, ret);
        printf("%s\n", ret);
    }
}
