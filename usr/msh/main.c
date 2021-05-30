#include <stdio.h>
#include <stdint.h>

#include <aos/aos.h>
#include <aos/udp_service.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("ERROR: no port given\n");
        return EXIT_SUCCESS;
    }
    int port = atoi(argv[1]);
    printf("starting msh on port %d\n", port);

    struct aos_socket sock;
    err = aos_socket_initialize(&sock, 0, 0, port);
    if (err_is_fail(err)) {
        printf("unable to initialize socket\n");
        return EXIT_SUCCESS;
    }

    return EXIT_SUCCESS;
}
