#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <devif/queue_interface_backend.h>
#include <devif/backends/net/enet_devif.h>
#include <aos/aos.h>
#include <aos/nameserver.h>
#include <aos/udp_service.h>
#include <aos/deferred.h>
#include <driverkit/driverkit.h>
#include <dev/imx8x/enet_dev.h>
#include <netutil/etharp.h>
#include <netutil/htons.h>
#include <netutil/ip.h>
#include <netutil/icmp.h>
#include <netutil/htons.h>
#include <netutil/checksum.h>
#include <netutil/udp.h>

#include <collections/hash_table.h>

#include "enet_regionman.h"
#include "enet.h"

// #define HANDLER_DEBUG_OPTION 1

#if defined(HANDLER_DEBUG_OPTION)
#define HAN_DEBUG(x...) debug_printf("[handler] " x);
#else
#define HAN_DEBUG(fmt, ...) ((void)0)
#endif

#define PANIC_IF_FAIL(err, msg)                 \
    if (err_is_fail(err)) {                     \
        USER_PANIC_ERR(err, msg);               \
    }

static struct udp_msg *repl_msg;
static char arp_tbl[2048];
static errval_t err;
__unused static uint16_t ping_seq;

static void udp_receive_handler_ns(struct enet_driver_state* st,
                                   struct udp_service_message *msg,
                                   void **response, size_t *response_bytes,
                                   struct aos_udp_socket *sock) {
    struct udp_recv_elem *ure = udp_socket_receive(sock);
    if (ure == NULL) {
        /* *response = NULL; */
        *response_bytes = 0;
        return;
    }

    size_t mln = sizeof(struct udp_msg) + ure->len * sizeof(char);
    struct udp_msg *rm = repl_msg;
    rm->f_port = ure->f_port;
    rm->len = ure->len;
    rm->ip = ure->ip_addr;
    memcpy(rm->data, ure->data, ure->len);

    free(ure->data);
    free(ure);
    *response = (void *) rm;
    *response_bytes = mln;
}

static void arp_tbl_handler_ns(struct enet_driver_state* st,
                              struct udp_service_message *msg,
                              void **response, size_t *response_bytes,
                              struct aos_udp_socket *sock) {
    if (collections_hash_traverse_start(st->arp_table) == -1) {
        // TODO: error!
    }
    char *res = arp_tbl;
    int ri = 0;  // index into res
    uint64_t key;
    uint32_t *curp = collections_hash_traverse_next(st->arp_table, &key);

    while (curp) {
        uint32_t ip_c = *curp;
        uint8_t ip_tbl[4];

        ip_tbl[3] = (ip_c >> 24) & 0xff;
        ip_tbl[2] = (ip_c >> 16) & 0xff;
        ip_tbl[1] = (ip_c >> 8) & 0xff;
        ip_tbl[0] = ip_c & 0xff;

        int tmp = sprintf(&res[ri], "%d.%d.%d.%d",
                          ip_tbl[3],
                          ip_tbl[2],
                          ip_tbl[1],
                          ip_tbl[0]);
        for (; tmp < 16; tmp++) {
            sprintf(&res[ri + tmp], " ");
        }
        ri += 16;
        ri += sprintf(&res[ri], "  ");
        ri += sprintf(&res[ri], "%x:%x:%x:%x:%x:%x\n",
                      (uint8_t) key >> 2,
                      (uint8_t) key >> 3,
                      (uint8_t) key >> 4,
                      (uint8_t) key >> 5,
                      (uint8_t) key >> 6,
                      (uint8_t) key >> 7);
        curp = collections_hash_traverse_next(st->arp_table, &key);
    }
    collections_hash_traverse_end(st->arp_table);
    res[ri++] = '\0';

    *response = res;
    *response_bytes = ri;
}

static void ping_send_handler_ns(struct enet_driver_state *st, uint32_t ip,
                                 void **response, size_t *response_bytes) {
    struct aos_icmp_socket *is = get_ping_socket(st, ip);
    if (is == NULL) {
        create_ping_socket(st, ip);
    }

    err = ping_socket_send_next(st, ip);
    *response = &err;
    *response_bytes = sizeof(errval_t);
}

static void ping_recv_handler_ns(struct enet_driver_state *st, uint32_t ip,
                                 void **response, size_t *response_bytes) {
    ping_seq = ping_socket_get_acked(st, ip);
    *response = &ping_seq;
    *response_bytes = sizeof(uint16_t);
}

static void server_recv_handler(void *stptr, void *message,
                                size_t bytes,
                                void **response, size_t *response_bytes,
                                struct capref rx_cap, struct capref *tx_cap)
{
    HAN_DEBUG("enet-nameserver-handler-called-debug-print-statement-reached-message\n");
    struct enet_driver_state *st = (struct enet_driver_state *) stptr;
    struct udp_service_message *msg = (struct udp_service_message *) message;
    struct aos_udp_socket *sock;
    struct udp_socket_create_info *usci;
    void *ptr;

    switch (msg->type) {
    case SEND:
        HAN_DEBUG("Send iiiiit\n");
        err = udp_socket_send(st, msg->port, msg->data, msg->len);
        *response = &err;
        *response_bytes = sizeof(errval_t);
        break;
    case RECV:
        HAN_DEBUG("Give plz\n");
        sock = get_socket_from_port(st, msg->port);
        if (sock == NULL) {
            *response_bytes = 0;
        } else {
            udp_receive_handler_ns(st, msg, response, response_bytes,
                                   sock);
        }
        break;
    case CREATE:
        HAN_DEBUG("Create\n");
        usci = (struct udp_socket_create_info *) msg->data;
        ptr = create_udp_socket(st, usci->ip_dest, usci->f_port, msg->port);
        HAN_DEBUG("==================== BP3\n");

        *response_bytes = sizeof(errval_t);

        err = ptr ? SYS_ERR_OK : LIB_ERR_NOT_IMPLEMENTED;
        *response = &err;
        HAN_DEBUG("==================== BP4\n");
        break;
    case DESTROY:
        HAN_DEBUG("kill it -.-\n");
        sock = get_socket_from_port(st, msg->port);
        err = udp_socket_teardown(st, sock);
        *response_bytes = sizeof(errval_t);
        *response = &err;
        break;
    case SEND_TO:
        HAN_DEBUG("Send iiit (to %d) \n", msg->ip);
        err = udp_socket_send_to(st, msg->port, msg->data, msg->len, msg->ip, msg->tgt_port);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "oh no :(");
            debug_printf("%d\n", err);
        }
        HAN_DEBUG("write repl\n");
        /* *response = (void *) err; */
        /* *response = NULL;  // TODO: error report sending */
        *response_bytes = 0;
        break;
    case ARP_TBL:
        HAN_DEBUG("ARP table\n");
        print_arp_table(st);
        arp_tbl_handler_ns(st, msg, response, response_bytes,
                                 sock);
        break;
    case ICMP_PING_SEND:
        ping_send_handler_ns(st, msg->ip, response, response_bytes);
        break;
    case ICMP_PING_RECV:
        ping_recv_handler_ns(st, msg->ip, response, response_bytes);
        break;
    }
}

void name_server_initialize(struct enet_driver_state *st) {
    errval_t err2;
    err2 = nameservice_register_properties(ENET_SERVICE_NAME, server_recv_handler, (void *) st, true,
                                          "type=ethernet,mac=TODO,bugs=0xffff");
    PANIC_IF_FAIL(err, "failed to register...\n");

    repl_msg = malloc(sizeof(struct udp_msg) + 2048 * sizeof(char));
}
