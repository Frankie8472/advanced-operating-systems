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
#include "enet.h"

// for icmp-packets
static char* icmp_payload = "Thro’ the ghoul-guarded gateways of slumber";
static const int icmp_plen = 44;

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
                                            uint16_t port) {
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
errval_t udp_socket_append_message(struct aos_udp_socket *s, uint16_t f_port, uint32_t ip,
                                   void *data, uint32_t len) {
    errval_t err = SYS_ERR_OK;

    struct udp_recv_elem *im = malloc(sizeof(struct udp_recv_elem));
    void *msg_ptr = malloc(sizeof(char) * len);
    im->data = msg_ptr;
    im->len = len;
    im->next = NULL;
    im->f_port = f_port;
    im->ip_addr = ip;
    memcpy(msg_ptr, data, len);

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

struct udp_recv_elem *udp_socket_receive(struct aos_udp_socket *s) {
    if (s->receive_buffer == NULL) {
        return NULL;
    }

    struct udp_recv_elem *res = s->receive_buffer;
    s->receive_buffer = res->next;
    if (s->last_elem == res) {
        s->last_elem = NULL;
    }

    return res;
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
 * on the provided driver-state. The created socket will already be inserted
 * into `st`.
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
 * \brief send an arp-request for the target ip-address.
 */
errval_t arp_request(struct enet_driver_state *st, uint32_t ip_to) {
    errval_t err = SYS_ERR_OK;
    struct devq_buf requ;
    err = get_free_buf(st->send_qstate, &requ);
    if (err_is_fail(err)) {
        ETHARP_DEBUG("could not get free buffer\n");
        return err;
    }

    // write eth-header for request
    struct region_entry *entry = get_region(st->txq, requ.rid);
    lvaddr_t raddr = (lvaddr_t) entry->mem.vbase + requ.offset + requ.valid_data;
    char *ra2 = (char *) raddr;
    memset(ra2 + 6, 0, 6);  // leave dest-mac empty -> dk
    ((struct eth_hdr *) ra2)->type = htons(ETH_TYPE_ARP);
    char *tmp = ra2 + 6;

    // write arp-header for request
    struct arp_hdr *ahr = (struct arp_hdr *) (ra2 + ETH_HLEN);
    ahr->hwtype = htons(ARP_HW_TYPE_ETH);
    ahr->proto = htons(ARP_PROT_IP);
    ahr->hwlen = 6;  // ?
    ahr->protolen = 4;  // ?
    ahr->opcode = htons(ARP_OP_REQ);

    uint8_t* macref = (uint8_t *) &(st->mac);
    for (int i = 0; i < 6; i++) {
        ahr->eth_src.addr[i] = macref[5 - i];
        tmp[i] = macref[5 - i];
    }

    ahr->ip_src = htonl(STATIC_ENET_IP);
    memset(&ahr->eth_dst, 0xff, 6);
    ahr->ip_dst = htonl(ip_to);

    requ.valid_length = ETH_HLEN + ARP_HLEN;

    dmb();

    ETHARP_DEBUG("=========== SENDING REQUEST\n");
    enqueue_buf(st->send_qstate, &requ);

    return SYS_ERR_OK;
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

    // get packeT
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
        UDP_DEBUG("but...where (not to)? %d\n", sock->ip_dest);
        print_arp_table(st);
        err = arp_request(st, sock->ip_dest);
        return err_is_fail(err) ? err : ENET_ERR_ARP_UNKNOWN;
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
    repl.valid_length = eth_tot_len;

    dmb();

    UDP_DEBUG("=========== SENDING MESSAGE\n");
    err = enqueue_buf(st->send_qstate, &repl);

    return err;
}

errval_t udp_socket_send_to(struct enet_driver_state *st, uint16_t port,
                            void *data, uint16_t len, uint32_t ip_to,
                            uint16_t port_to) {
    UDP_DEBUG("sending to\n");
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

    UDP_DEBUG("found socket\n");

    // get packet
    struct devq_buf repl;
    err = get_free_buf(st->send_qstate, &repl);
    if (err_is_fail(err)) {
        UDP_DEBUG("un4ble 2 get free buffer\n");
        return err;
    }

    UDP_DEBUG("getting buf and so on\n");
    struct region_entry *entry = get_region(st->txq, repl.rid);
    lvaddr_t maddr = (lvaddr_t) entry->mem.vbase + repl.offset + repl.valid_data;
    struct eth_hdr *meh = (struct eth_hdr *) maddr;
    uint64_t *mac_tgt = collections_hash_find(st->inv_table, ip_to);

    if (mac_tgt == NULL) {
        UDP_DEBUG("but...where? %d\n", ip_to);
        print_arp_table(st);
        err = arp_request(st, ip_to);
        return err_is_fail(err) ? err : ENET_ERR_ARP_UNKNOWN;
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
    mih->dest = htonl(ip_to);
    mih->chksum = 0;
    mih->chksum = inet_checksum(mih, IP_HLEN);

    // write UDP header
    UDP_DEBUG("writing UDP header\n");
    struct udp_hdr *muh = (struct udp_hdr *) ((char *) mih + IP_HLEN);
    muh->src = htons(port);
    muh->dest = htons(port_to);
    muh->len = htons(eth_tot_len - ETH_HLEN - IP_HLEN);
    muh->chksum = 0;

    // copy payload
    UDP_DEBUG("writing payload\n");
    memcpy((char *) muh + UDP_HLEN, data, len);
    repl.valid_length = eth_tot_len;

    dmb();

    UDP_DEBUG("=========== SENDING MESSAGE\n");
    err = enqueue_buf(st->send_qstate, &repl);

    return err;
}

/**
 * \brief retrieve reference to ping socket pinging the provided ip address
 * \return reference to a ping socket, or NULL if none could be found
 */
struct aos_icmp_socket *get_ping_socket(struct enet_driver_state *st, uint32_t ip) {
    for (struct aos_icmp_socket *i = st->pings;
         i; i = i->next) {
        if (i->ip == ip) {
            return i;
        }
    }
    return NULL;
}

/**
 * \brief create a new icmp-socket for the provided ip address.
 * \return reference to the created socket, or NULL if it couldn't be created
 */
struct aos_icmp_socket *create_ping_socket(struct enet_driver_state *st, uint32_t ip) {
    if (get_ping_socket(st, ip)) {
        return NULL;
    }

    struct aos_icmp_socket *nu = malloc(sizeof(struct aos_icmp_socket));
    nu->ip = ip;
    nu->id_mask = 0xf11f;  // arbitrary default
    nu->seq_sent = 0;
    nu->seq_rcv = 0;
    nu->next = st->pings;
    st->pings = nu;
    return nu;
}

/**
 * \brief tear down the ping socket for the provided ip address
 */
errval_t ping_socket_teardown(struct enet_driver_state *st, uint32_t ip) {
    struct aos_icmp_socket *prev = st->pings;

    // is the socket in question first?
    if (prev && prev->ip == ip) {
        st->pings = prev->next;
        free(prev);
        return SYS_ERR_OK;
    }

    for (; prev; prev = prev->next) {
        if (prev->next && prev->next->ip == ip) {
            break;
        }
    }

    if (prev) {
        // previous socket found
        struct aos_icmp_socket *to_kill = prev->next;
        prev->next = to_kill->next;
        free(to_kill);
    }
    return SYS_ERR_OK;
}

/**
 * \brief send the next icmp-packet from the socket on the provided port
 * if all sent packets have been acked, it sends the next higher one
 * otherwise, it resends the current one
 */
errval_t ping_socket_send_next(struct enet_driver_state *st, uint32_t ip) {
    // retrieve icmp-socket
    errval_t err = SYS_ERR_OK;
    struct aos_icmp_socket *is = get_ping_socket(st, ip);
    if (is == NULL) {
        return ENET_ERR_NO_SOCKET;
    }

    // check if arp known
    uint64_t *mac_tgt = collections_hash_find(st->inv_table, ip);
    if (mac_tgt == NULL) {
        ICMP_DEBUG("unknown ping-destination %d\n", ip);
        err = arp_request(st, ip);
        return err_is_fail(err) ? err : ENET_ERR_ARP_UNKNOWN;
    }

    uint16_t seqno;
    if (is->seq_sent == is->seq_rcv) {
        seqno = ++is->seq_sent;
    } else {
        seqno = is->seq_sent;
    }

    uint16_t pkid = seqno ^ is->id_mask;

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

    // write ETH header
    ICMP_DEBUG("writing ETH header\n");
    u64_to_eth_addr(*mac_tgt, &meh->dst);
    uint8_t* macref = (uint8_t *) &(st->mac);
    for (int i = 0; i < 6; i++) {
        meh->src.addr[i] = macref[5 - i];
    }
    meh->type = htons(ETH_TYPE_IP);

    // write IP header
    ICMP_DEBUG("writing IP header\n");
    struct ip_hdr *mih = (struct ip_hdr *) ((char *) meh + ETH_HLEN);
    IPH_VHL_SET(mih, 4, 5);
    mih->tos = 0;  // ?
    mih->len = htons(IP_HLEN + ICMP_HLEN + icmp_plen);
    mih->id = htons(pkid);
    mih->offset = 0;
    mih->ttl = 64;
    mih->proto = IP_PROTO_ICMP;
    mih->src = htonl(STATIC_ENET_IP);
    mih->dest = htonl(ip);
    mih->chksum = 0;
    mih->chksum = inet_checksum(mih, IP_HLEN);

    // write ICMP header
    ICMP_DEBUG("writing ICMP header\n");
    struct icmp_echo_hdr *mieh = (struct icmp_echo_hdr *) ((char *) mih + IP_HLEN);
    mieh->type = ICMP_ECHO;
    mieh->code = 0;  // ?
    mieh->chksum = 0;
    mieh->id = htons(pkid);
    mieh->seqno = htons(seqno);

    memcpy((void *) ((char *) mieh) + ICMP_HLEN, icmp_payload,
           icmp_plen);
    mieh->chksum = (inet_checksum((char *) mieh, icmp_plen + ICMP_HLEN));

    repl.valid_length = ETH_HLEN + IP_HLEN + ICMP_HLEN + icmp_plen;

    dmb();

    ICMP_DEBUG("=========== SENDING REQUEST\n");
    enqueue_buf(st->send_qstate, &repl);

    return err;
}

/**
 * \brief get the highest acked packet for the given ip's ping-socket
 */
uint16_t ping_socket_get_acked(struct enet_driver_state *st, uint32_t ip) {
    struct aos_icmp_socket *is = get_ping_socket(st, ip);
    if (is == NULL) {
        return 0;
    }

    return is->seq_rcv;
}
