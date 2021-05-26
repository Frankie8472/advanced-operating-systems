#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <devif/queue_interface_backend.h>
#include <devif/backends/net/enet_devif.h>
#include <aos/aos.h>
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

// NOTE: defined in other thingy too but nis
static struct region_entry* get_region(struct enet_queue* q, regionid_t rid)
{
    struct region_entry* entry = q->regions;
    while (entry != NULL) {
        if (entry->rid == rid) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/* /\** */
/*  * \brief given an id and a driver state, retrieve the corresponding udp socket. */
/*  * \return reference to the according udp socket, NULL if none could be found */
/*  *\/ */
/* struct aos_udp_socket* get_socket_from_id(struct enet_driver_state *st, */
/*                                           uint64_t socket_id) { */
/*     for (struct aos_udp_socket *i = st->sockets; i; i = i->next) { */
/*         if (i->sock_id == socket_id) */
/*             return i; */
/*     } */
/*     return NULL; */
/* } */

/**
 * \brief given a port and a driver state, retrieve the corresponding udp socket
 * that is listening for incoming traffic on that port.
 * \return reference to the according udp socket, NULL if none could be found
 */
struct aos_udp_socket* get_socket_from_port(struct enet_driver_state *st,
                                            uint32_t port) {
    for (struct aos_udp_socket *i = st->sockets; i; i = i->next) {
        if (i->l_port == port)
            return i;
    }
    return NULL;
}

/**
 * \brief adds a new incoming message to the provided udp-socket.
 * \param data pointer to the incoming data to append
 * \param len length of the incoming message, in bytes
 * NOTE: the length should already be adjusted for the udp-header length
 * it should only describe the payload-length, without any headers.
 */
errval_t udp_socket_append_message(struct aos_udp_socket *s, void *data,
                                   uint32_t len) {
    errval_t err = SYS_ERR_OK;

    struct udp_recv_elem *im = malloc(sizeof(struct udp_recv_elem));
    void *ip = malloc(sizeof(char) * len);
    im->data = ip;
    im->len = len;
    im->next = NULL;
    memcpy(ip, data, len);

    struct udp_recv_elem *prev_last = s->last_elem;
    if (prev_last) {
        prev_last->next = im;
        s->last_elem = im;
    } else {
        s->receive_buffer = im;
        s->last_elem = im;
    }
    return err;
}

/**
 * \brief tears down an udp socket: deletes it from the driver state,
 * frees all data connected to its state.
 */
errval_t udp_socket_teardown(struct enet_driver_state *st,
                             struct aos_udp_socket *socket) {
    // remove from linked list
    if (st->sockets == socket) {  // easy case
        st->sockets = socket->next;
    } else {
        struct aos_udp_socket *prev = st->sockets;
        for (; prev->next && prev->next != socket; prev = prev->next)
            ;

        if (prev) {
            prev->next = socket->next;
        } else {
            // should not happen
        }
    }

    // free all remaining data from in-buffer
    for (struct udp_recv_elem *i = socket->receive_buffer;
         i; i = i->next) {
        free(i->data);
        free(i);
    }

    // free socket itself
    free(socket);
    return SYS_ERR_OK;
}

/**
 * \brief create a new udp-socket with given souce-port, destination-ip/port
 * on the provided driver-state.
 * \return reference to the new socket, if it was created. If it was not
 * created (presumably because a port with the same local port already exists),
 * return NULL instead.
 */
struct aos_udp_socket* create_udp_socket(struct enet_driver_state *st,
                                         uint32_t ip_dest, uint16_t f_port,
                                         uint16_t l_port) {
    struct aos_udp_socket *ex = get_socket_from_port(st, l_port);
    if (ex) {  // does another socket on that port aready exist?
        return NULL;
    }

    struct aos_udp_socket *nu = malloc(sizeof(struct aos_udp_socket));
    nu->ip_dest = ip_dest;
    nu->f_port = f_port;
    nu->l_port = l_port;
    nu->receive_buffer = NULL;
    nu->last_elem = NULL;
    nu->next = st->sockets;
    st->sockets = nu;

    return nu;
}

/**
 * \brief send a UDP message over the provided port.
 * \param port outgoing port of the message.
 * NOTE: since only one connection per port is allowed, this
 * uniquely defines a UDP connection, including destination information.
 * \param data pointer to the payload to send
 * \param len length of the payload. This implementation does not (yet)
 * support fragmentation etc. If the payload is too long for a single
 * UDP message, it will simply be truncated.
 */
errval_t udp_socket_send(struct enet_driver_state *st, uint16_t port,
                         void *data, uint16_t len) {
    static uint16_t generic_id = 5555;  // NOTE: maybe store in socket obj instead
    // possibly truncate len
    if (len > UDP_SOCK_MAX_LEN)
        len = UDP_SOCK_MAX_LEN;
    const uint16_t eth_tot_len = len + ETH_HLEN + IP_HLEN + UDP_HLEN;

    errval_t err;
    struct aos_udp_socket *sock = get_socket_from_port(st, port);
    if (sock == NULL) {
        return ENET_ERR_NO_SOCKET;
    }

    // get packet
    struct devq_buf repl;
    err = get_free_buf(st->send_qstate, &repl);
    if (err_is_fail(err)) {
        UDP_DEBUG("un4ble 2 get free buffer\n");
        return err;
    }

    struct region_entry *entry = get_region(st->txq, repl.rid);
    lvaddr_t maddr = (lvaddr_t) entry->mem.vbase + repl.offset + repl.valid_data;
    struct eth_hdr *meh = (struct eth_hdr *) maddr;
    uint64_t *mac_tgt = collections_hash_find(st->inv_table, sock->ip_dest);

    if (mac_tgt == NULL) {
        return ENET_ERR_ARP_UNKNOWN;
    }

    // write ETH header
    UDP_DEBUG("writing ETH header\n");
    u64_to_eth_addr(*mac_tgt, &meh->dst);
    uint8_t* macref = (uint8_t *) &(st->mac);
    for (int i = 0; i < 6; i++) {
        meh->src.addr[i] = macref[5 - i];
    }
    meh->type = htons(ETH_TYPE_IP);

    // write IP header
    UDP_DEBUG("writing IP header\n");
    struct ip_hdr *mih = (struct ip_hdr *) ((char *) meh + ETH_HLEN);
    IPH_VHL_SET(mih, 4, 5);
    mih->tos = 0;  // ?
    mih->len = htons(eth_tot_len - ETH_HLEN);
    mih->id = ++generic_id;
    mih->offset = 0;
    mih->ttl = 64;
    mih->proto = IP_PROTO_UDP;
    mih->src = htonl(STATIC_ENET_IP);
    mih->dest = htonl(sock->ip_dest);
    mih->chksum = 0;
    mih->chksum = inet_checksum(mih, IP_HLEN);

    // write UDP header
    UDP_DEBUG("writing UDP header\n");
    struct udp_hdr *muh = (struct udp_hdr *) ((char *) mih + IP_HLEN);
    muh->src = htons(port);
    muh->dest = htons(sock->f_port);
    muh->len = htons(len);
    muh->chksum = 0;

    // copy payload
    UDP_DEBUG("writing payload\n");
    memcpy((char *) muh + UDP_HLEN, data, len);

    dmb();

    UDP_DEBUG("=========== SENDING MESSAGE\n");
    err = enqueue_buf(st->send_qstate, &repl);

    return err;
}
