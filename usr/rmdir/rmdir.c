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
    if (argc == 2) {
        char *path = argv[1];
        delete_dir(path);
        return 0;
    }
}
