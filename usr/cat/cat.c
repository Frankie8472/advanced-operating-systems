/**
 * \file
 * \brief cat application
 */

#include <aos/aos.h>
#include <aos/aos_datachan.h>
#include <aos/dispatcher_rpc.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    char buffer[1];
    size_t read;
    do {
        read = fread(buffer, 1, sizeof buffer, stdin);
        fwrite(buffer, 1, read, stdout);
        fflush(stdout);
    } while (read > 0);

    return 0;
}
