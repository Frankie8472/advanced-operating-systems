/**
 * \file
 * \brief wtf application
 */

#include <aos/aos.h>
#include <aos/aos_datachan.h>
#include <aos/dispatcher_rpc.h>
#include <stdio.h>
#include <aos/fs_service.h>

int main(int argc, char *argv[])
{
    if (argc == 3) {
        char *path = argv[0];
        char *text = argv[1];
        write_file(path, data);
        return 0;
    }
}
