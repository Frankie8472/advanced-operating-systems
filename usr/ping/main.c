#include <stdio.h>
#include <stdint.h>

#include <aos/aos.h>
#include <aos/udp_service.h>

int main(int argc, char **argv) {
    errval_t err;
    if (argc < 3) {
        printf("invalid number of arguments!\n");
        printf("usage:\n"
               "$ ping `IP` `COUNT`\n"
               "example:\n"
               "$ ping 192.168.1.1 500\n");
        return EXIT_SUCCESS;
    }

    // parse ip
    char curn[20];
    uint32_t tgt_ip = 0;
    int curi = 0;
    int lim = strlen(argv[1]);
    for (int i = 0; i < 4; i++) {
        int ti = 0;
        while (curi < lim && argv[1][curi] != '.') {
            curn[ti++] = argv[1][curi++];
        }
        curi++;

        curn[ti] = '\0';
        tgt_ip |= atoi(curn) << (3 - i) * 8;
    }

    int count = atoi(argv[2]);

    printf("pinging %s, %d times\n", argv[1], count);

    struct aos_ping_socket sock;
    err = aos_ping_init(&sock, tgt_ip);
    if (err_is_fail(err)) {
        printf("unable to initialize ping-connection\n");
        return EXIT_SUCCESS;
    }

    int cnnt = 0;
    do {  // wait til setup
        err = aos_ping_send(&sock);
        thread_yield();
        cnnt++;
    } while (err == ENET_ERR_ARP_UNKNOWN && cnnt < 500);
    if (cnnt == 500) {
        printf("unable to resolve ARP information\n");
        return EXIT_SUCCESS;
    }

    uint16_t ackd = 0;

    while (ackd < count) {
        aos_ping_send(&sock);
        thread_yield();
        uint16_t a2 = aos_ping_recv(&sock);
        if (a2 > ackd) {
            printf("packet %d was acked\n", a2);
            ackd = a2;
        }
    }

    return EXIT_SUCCESS;
}
