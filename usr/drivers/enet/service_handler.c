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
#include "enet_handler.h"
#include "enet.h"

#define PANIC_IF_FAIL(err, msg)                 \
    if (err_is_fail(err)) {                     \
        USER_PANIC_ERR(err, msg);               \
    }

static void server_recv_handler(void *stptr, void *message,
                                size_t bytes,
                                void **response, size_t *response_bytes,
                                struct capref rx_cap, struct capref *tx_cap)
{
    debug_printf("enet-nameserver-handler-called-debug-print-statement-reached-message\n");
    errval_t err;
    struct enet_driver_state *st = (struct enet_driver_state *) stptr;
    struct udp_service_message *msg = (struct udp_service_message *) message;
    struct aos_udp_socket *sock;
    uint16_t mln;
    struct udp_socket_create_info *usci;

    switch (msg->type) {
    case SEND:
        err = udp_socket_send(st, msg->port, msg->data, msg->len);
        *response = (void *) err;
        response_bytes = 0;
        break;
    case RECV:
        sock = get_socket_from_port(st, msg->port);
        if (sock == NULL) {
            *response = NULL;
            *response_bytes = 0;
        } else {
            *response = udp_socket_receive(sock, &mln);
            *response_bytes = mln;
        }
        break;
    case CREATE:  // TODO: reporting
        usci = (struct udp_socket_create_info *) msg->data;
        create_udp_socket(st, usci->ip_dest, usci->f_port, msg->port);
        *response = NULL;
        *response_bytes = 0;
        break;
    case DESTROY:
        sock = get_socket_from_port(st, msg->port);
        udp_socket_teardown(st, sock);
        break;
    }
}

void name_server_initialize(struct enet_driver_state *st) {
    errval_t err;
    debug_printf("let's try\n");
    err = nameservice_register_properties("/ethernet", server_recv_handler, (void *) st, true,
                                          "type=ethernet,mac=TODO,bugs=0xffff");
    PANIC_IF_FAIL(err, "failed to register...\n");
}
