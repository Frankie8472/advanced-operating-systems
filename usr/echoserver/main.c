#include <stdio.h>
#include <stdint.h>

#include <aos/aos.h>
#include <aos/nameserver.h>
#include <aos/udp_service.h>
#define MK_IP(a,b,c,d) (((a)<<24)|((b)<<16)|((c)<<8)|(d))

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("invalid number of arguments! EXIT\n");
        return EXIT_SUCCESS;
    }

    int port = atoi(argv[1]);

    errval_t err;
    printf("starting echo-server on port %d\n", port);

    struct aos_socket sock;
    err = aos_socket_initialize(&sock, 0, 0, port);
    if (err_is_fail(err)) {
        printf("unable to initialize socket\n");
        return EXIT_SUCCESS;
    }

    /* uint32_t tip = MK_IP(192, 168, 1, 34); */
    /* aos_socket_send_to(&sock, "hello im here", 13, tip, 8888); */
    struct udp_msg *in = malloc(sizeof(struct udp_msg) + 2048 * sizeof(char));
    while (1) {
        err = aos_socket_receive(&sock, in);

        if (err_is_fail(err)) {
            continue;
        }

        printf("received message:\n%s", in->data);
        aos_socket_send_to(&sock, in->data, in->len, in->ip, in->f_port);
    }

    aos_socket_teardown(&sock);
    free(in);
    return EXIT_SUCCESS;
}
