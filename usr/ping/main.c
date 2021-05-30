#include <stdio.h>
#include <stdint.h>

#include <aos/aos.h>
#include <aos/udp_service.h>

#define MK_IP(a,b,c,d) (((a)<<24)|((b)<<16)|((c)<<8)|(d))
int main(int argc, char **argv) {
    errval_t err;
    /* uint32_t tgt_ip = MK_IP(169, 254, 6, 85); */
    uint32_t tgt_ip = MK_IP(192, 168, 1, 34);
    printf("pinging %d\n", tgt_ip);
    struct aos_ping_socket sock;
    err = aos_ping_init(&sock, tgt_ip);
    if (err_is_fail(err)) {
        printf("unable to initialize ping-connection\n");
        return EXIT_SUCCESS;
    }

    do {  // wait til setup
        err = aos_ping_send(&sock);
    } while (err == ENET_ERR_ARP_UNKNOWN);

    uint16_t ackd = 0;

    for (int i = 0; i < 10//00000
             ; i++) {
        aos_ping_send(&sock);
        uint16_t a2 = aos_ping_recv(&sock);
        if (a2 > ackd) {
            printf("received packed %d!\n", a2);
            ackd = a2;
            break;
        }
    }

    return EXIT_SUCCESS;
}
