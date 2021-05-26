#include <stdio.h>
#include <stdint.h>

#include <aos/aos.h>
#include <aos/nameserver.h>
#include <aos/udp_service.h>
#define PORT 184

int main(int argc, char **argv) {
    errval_t err;
    printf("starting echo-server on port %d\n", PORT);

    struct aos_socket sock;
    err = aos_socket_initialize(&sock, 0, 0, PORT);
    if (err_is_fail(err)) {
        printf("unable to initialize socket\n");
        return 1;
    }

    struct udp_msg *in;
    while (1) {
        in = aos_socket_receive(&sock);

        if (in == NULL) {
            continue;
        }

        printf("RECEIVED A MESSAGE!!!!\n");
        aos_socket_send_to(&sock, in->data, in->len, in->ip, in->f_port);
    }
}
