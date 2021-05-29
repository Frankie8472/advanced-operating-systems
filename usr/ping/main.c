#include <stdio.h>
#include <stdint.h>

#include <aos/aos.h>
#include <aos/udp_service.h>

#define MK_IP(a,b,c,d) (((a)<<24)|((b)<<16)|((c)<<8)|(d))
int main(int argc, char **argv) {
    errval_t err;
    uint32_t tgt_ip = MK_IP(192, 168, 1, 34);
    printf("pinging %d\n", tgt_ip);
    struct aos_ping_socket sock;
    err = aos_ping_init(&sock, tgt_ip);
    if (err_is_fail(err)) {
        printf("unable to initialize ping-connection\n");
        return EXIT_SUCCESS;
    }

    do {
        err = aos_ping_send(&sock);
        printf("hehehe\n");
    } while (err == ENET_ERR_ARP_UNKNOWN);

    if (err == ENET_ERR_ARP_UNKNOWN) {
        debug_printf("how did i get here?");
    }
    DEBUG_ERR(err, "this is it\n");
    debug_printf("err is %d, not %d\n", err, ENET_ERR_ARP_UNKNOWN);

    return EXIT_SUCCESS;
}
