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
#include <netutil/ip.h>
#include <netutil/icmp.h>
#include <netutil/htons.h>
#include <netutil/checksum.h>
#include <netutil/udp.h>

#include <collections/hash_table.h>

#include "enet.h"
#include "enet_regionman.h"
#include "enet_handler.h"

__attribute__((__unused__))
static void print_arp_table(struct enet_driver_state *st) {
    if (collections_hash_traverse_start(st->arp_table) == -1) {
        ENET_DEBUG("unable to print arp-table rn\n");
        return;
    }

    ENET_DEBUG("============ ARP Table ============\n");
    uint64_t key;
    uint32_t *curp = collections_hash_traverse_next(st->arp_table, &key);
    while (curp) {
        uint32_t ip_c = *curp;
        uint8_t ip_tbl[4];
        
        ip_tbl[3] = (ip_c >> 24) & 0xff;
        ip_tbl[2] = (ip_c >> 16) & 0xff;
        ip_tbl[1] = (ip_c >> 8) & 0xff;
        ip_tbl[0] = ip_c & 0xff;

        ENET_DEBUG("%d.%d.%d.%d --- %x:%x:%x:%x:%x:%x\n",
                   ip_tbl[3],
                   ip_tbl[2],
                   ip_tbl[1],
                   ip_tbl[0],
                   (uint8_t) key >> 2,
                   (uint8_t) key >> 3,
                   (uint8_t) key >> 4,
                   (uint8_t) key >> 5,
                   (uint8_t) key >> 6,
                   (uint8_t) key >> 7
            );

        curp = collections_hash_traverse_next(st->arp_table, &key);
    }

    ENET_DEBUG("===================================\n");
    collections_hash_traverse_end(st->arp_table);
}

static void inline deb_print_mac(char *msg, struct eth_addr* mac) {
    ENET_DEBUG("%s: %x:%x:%x:%x:%x:%x\n",
               msg,
               mac->addr[0],
               mac->addr[1],
               mac->addr[2],
               mac->addr[3],
               mac->addr[4],
               mac->addr[5]);
}

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

static void print_ip_packet(struct ip_hdr *ih) {
    IP_DEBUG("======== IP  PACKET ========\n");
    IP_DEBUG("version - %d\n", IPH_V(ih));
    IP_DEBUG("header length - %d\n", IPH_HL(ih));
    // TODO: tos
    IP_DEBUG("length - %d\n", ntohs(ih->len));
    IP_DEBUG("id - %d\n", ntohs(ih->id));
    IP_DEBUG("offset - %d\n", ntohs(ih->offset));
    IP_DEBUG("ttl - %d\n", ih->ttl);
    IP_DEBUG("protocol - %d\n", ih->proto);
    ENET_DEBUG_UI32_AS_IP("src:", ntohl(ih->src));
    ENET_DEBUG_UI32_AS_IP("dest:", ntohl(ih->dest));
}

static void print_arp_packet(struct arp_hdr* h) {
    ETHARP_DEBUG("======== ARP PACKET ========\n");
    ETHARP_DEBUG("hwtype - %d\n", ntohs(h->hwtype));
    ETHARP_DEBUG("proto - %d\n", ntohs(h->proto));
    ETHARP_DEBUG("hwlen - %d\n", h->hwlen);
    ETHARP_DEBUG("protolen - %d\n", h->protolen);
    ETHARP_DEBUG("opcode - %d\n", ntohs(h->opcode));
    ENET_DEBUG_UI32_AS_IP("ip_src -", ntohl(h->ip_src));
    ENET_DEBUG_UI32_AS_IP("ip_dst -", ntohl(h->ip_dst));
    deb_print_mac("eth_src", &h->eth_src);
    deb_print_mac("eth_dst", &h->eth_dst);
}

static errval_t arp_request_handle(struct enet_queue* q, struct devq_buf* buf,
                                   struct arp_hdr *h, struct enet_driver_state* st,
                                   lvaddr_t original_header) {
    if (ntohl(h->ip_dst) != STATIC_ENET_IP) {
        return SYS_ERR_OK;
    }

    ETHARP_DEBUG("is for me?\n");
    ETHARP_DEBUG("(o--o)\n");
    ETHARP_DEBUG("|    |\n");
    ETHARP_DEBUG("`-><-'\n");

    // extract src-info
    errval_t err;
    uint64_t mac_src = eth_addr_to_u64(&h->eth_src);
    uint32_t *ip_src_ref = malloc(sizeof(uint32_t));
    *ip_src_ref = ntohl(h->ip_src);

    uint32_t *stored = collections_hash_find(st->arp_table, mac_src);
    if (stored) {
        if (*stored != *ip_src_ref) {
            ETHARP_DEBUG("updating ARP table entry\n");
            collections_hash_delete(st->arp_table, mac_src);
            collections_hash_insert(st->arp_table, mac_src, ip_src_ref);
        } else {
            ETHARP_DEBUG("ARP entry already stored\n");
        }
    } else {
        // save info in arp-table
        ETHARP_DEBUG("adding new ARP table entry\n");
        collections_hash_insert(st->arp_table, mac_src, ip_src_ref);
    }

    // reply to it
    struct devq_buf repl;
    err = get_free_buf(st->send_qstate, &repl);
    if (err_is_fail(err)) {
        ETHARP_DEBUG("could not get free buffer\n");
        return err;
    }

    // write eth-header for reply
    struct region_entry *entry = get_region(st->txq, repl.rid);
    lvaddr_t raddr = (lvaddr_t) entry->mem.vbase + repl.offset + repl.valid_data;
    char *ra2 = (char *) raddr;
    char *oh2 = (char *) original_header;
    memcpy(ra2, oh2 + 6, 6);
    memcpy(ra2 + 6, oh2, 6);
    memcpy(ra2 + 12, oh2 + 12, 2);

    // write arp-header for reply
    struct arp_hdr *ahr = (struct arp_hdr *) (ra2 + ETH_HLEN);
    ahr->hwtype = h->hwtype;
    ahr->proto = h->proto;
    ahr->hwlen = h->hwlen;
    ahr->protolen = h->protolen;
    ahr->opcode = htons(ARP_OP_REP);
    /* memcpy(&ahr->eth_src, &st->mac, 6); */
    // NOTE: do I have to worry bout no/ho?
    uint8_t* macref = (uint8_t *) &(st->mac);
    for (int i = 0; i < 6; i++) {
        ahr->eth_src.addr[i] = macref[5 - i];
    }
    ETHARP_DEBUG("IPSDF: %x\n", st->mac);
    deb_print_mac("macbac", &ahr->eth_src);
    ahr->ip_src = htonl(STATIC_ENET_IP);
    memcpy(&ahr->eth_dst, &h->eth_src, 6);
    ahr->ip_dst = h->ip_src;

    repl.valid_length = ETH_HLEN + ARP_HLEN;

    dmb();

    ETHARP_DEBUG("=========== SENDING REPLYYY\n");
    enqueue_buf(st->send_qstate, &repl);

    return SYS_ERR_OK;
}

static errval_t arp_reply_handle(struct enet_queue* q, struct devq_buf* buf,
                                 struct arp_hdr *h, struct enet_driver_state* st,
                                 lvaddr_t original_header) {
    errval_t err = SYS_ERR_OK;
    // check if designated for this device
    if (ntohl(h->ip_dst) != STATIC_ENET_IP) {
        ETHARP_DEBUG("not for me D:\n");
        return err;
    }

    // if for us, just save contained information
    ETHARP_DEBUG("reply for me :D\n");
    uint64_t mac_src = eth_addr_to_u64(&h->eth_src);
    uint32_t *ip_src_ref = malloc(sizeof(uint32_t));
    *ip_src_ref = ntohl(h->ip_src);

    uint32_t *stored = collections_hash_find(st->arp_table, mac_src);
    if (stored) {
        if (*stored != *ip_src_ref) {
            ETHARP_DEBUG("updating ARP table entry\n");
            collections_hash_delete(st->arp_table, mac_src);
            collections_hash_insert(st->arp_table, mac_src, ip_src_ref);
        } else {
            ETHARP_DEBUG("ARP entry already stored\n");
        }
    } else {
        // save info in arp-table
        ETHARP_DEBUG("adding new ARP table entry\n");
        collections_hash_insert(st->arp_table, mac_src, ip_src_ref);
    }

    return err;
}

errval_t handle_ARP(struct enet_queue* q, struct devq_buf* buf,
                    lvaddr_t vaddr, struct enet_driver_state* st) {
    errval_t err = SYS_ERR_OK;
    struct arp_hdr *header = (struct arp_hdr*) ((char *) vaddr + ETH_HLEN);
    print_arp_packet(header);

    switch (ntohs(header->opcode)) {
    case ARP_OP_REQ:
        ETHARP_DEBUG("ARP Request\n");
        err = arp_request_handle(q, buf, header, st, vaddr);
        break;
    case ARP_OP_REP:
        ETHARP_DEBUG("ARP Reply\n");
        err = arp_reply_handle(q, buf, header, st, vaddr);
        break;
    default:
        ETHARP_DEBUG("Unknown ARP opcode\n");
        // TODO: add errorcode to return here maybe?
        break;
    }
    /* print_arp_table(st); */
    return err;
}

static errval_t icmp_echo_handle(
    struct enet_queue* q, struct devq_buf* buf,
    struct icmp_echo_hdr *h, struct enet_driver_state* st,
    lvaddr_t original_header) {
    // extract some info
    errval_t err = SYS_ERR_OK;
    ICMP_DEBUG("id - %d\n", ntohs(h->id));
    ICMP_DEBUG("seqno - %d\n", ntohs(h->seqno));

    // send reply
    struct devq_buf repl;
    err = get_free_buf(st->send_qstate, &repl);
    if (err_is_fail(err)) {
        ICMP_DEBUG("un4ble 2 get free buffer\n");
        return err;
    }

    struct region_entry *entry = get_region(st->txq, repl.rid);
    lvaddr_t raddr = (lvaddr_t) entry->mem.vbase + repl.offset + repl.valid_data;
    struct eth_hdr *oeh = (struct eth_hdr *) original_header;
    struct eth_hdr *reh = (struct eth_hdr *) raddr;

    // set src / dst of eth header
    memcpy(&reh->src, &oeh->dst, 6);
    memcpy(&reh->dst, &oeh->src, 6);
    reh->type = oeh->type;

    // set ip-header
    struct ip_hdr *oih = (struct ip_hdr *) ((char *) oeh + ETH_HLEN);
    struct ip_hdr *rih = (struct ip_hdr *) ((char *) reh + ETH_HLEN);


    IPH_VHL_SET(rih, 4, 5);  // NOTE: not sure but otpimistic i guess?
    rih->tos = oih->tos;
    // NOTE: len
    rih->len = oih->len;
    rih->id = oih->id;
    /* rih->id |= htons(0x4000); */
    rih->offset = htons(0x4000);
    rih->ttl = oih->ttl;
    rih->proto = oih->proto;
    rih->src = oih->dest;
    rih->dest = oih->src;

    // set icmp-header
    struct icmp_echo_hdr *och = (struct icmp_echo_hdr *) ((char *) oih + IP_HLEN);
    struct icmp_echo_hdr *rch = (struct icmp_echo_hdr *) ((char *) rih + IP_HLEN);
    rch->type = ICMP_ER;
    rch->code = och->code;
    // NOTE: chksum
    rch->chksum = 0;
    rch->id = och->id;
    rch->seqno = och->seqno;

    // payload
    memcpy((void *) ((char *) rch) + ICMP_HLEN, (void *) ((char *) och) + ICMP_HLEN,
           ntohs(oih->len) - ICMP_HLEN - IP_HLEN);

    rch->chksum = (inet_checksum((char *) rch, ntohs(oih->len) - IP_HLEN));

    // IP chksum
    rih->chksum = 0;
    rih->chksum = (inet_checksum((void *) rih, IP_HLEN));
    /* rih->chksum = htons(inet_checksum((void *) rch, ntohs(oih->len) - IP_HLEN)); */

    repl.valid_length = ETH_HLEN + IP_HLEN + htons(rih->len) - IP_HLEN;

    dmb();

    ICMP_DEBUG("=========== SENDING REPLYYY\n");
    enqueue_buf(st->send_qstate, &repl);

    return err;
}

static errval_t handle_ICMP(struct enet_queue* q, struct devq_buf* buf,
                            struct icmp_echo_hdr *h, struct enet_driver_state* st,
                            lvaddr_t original_header) {
    errval_t err = SYS_ERR_OK;

    switch (ICMPH_TYPE(h)) {
    case ICMP_ECHO:
        ICMP_DEBUG("handling ICMP Echo request\n");
        return icmp_echo_handle(q, buf, h, st, original_header);
    default:
        ICMP_DEBUG("unaccounted ICMP type %d\n", ICMPH_TYPE(h));
        return LIB_ERR_NOT_IMPLEMENTED;
    }

    return err;
}

static errval_t udp_echo(struct enet_queue* q, struct devq_buf* buf,
                         struct udp_hdr *h, struct enet_driver_state* st,
                         lvaddr_t original_header) {
    errval_t err = SYS_ERR_OK;
    
    // send reply
    struct devq_buf repl;
    err = get_free_buf(st->send_qstate, &repl);
    if (err_is_fail(err)) {
        UDP_DEBUG("un4ble 2 get free buffer\n");
        return err;
    }

    struct region_entry *entry = get_region(st->txq, repl.rid);
    lvaddr_t raddr = (lvaddr_t) entry->mem.vbase + repl.offset + repl.valid_data;
    struct eth_hdr *oeh = (struct eth_hdr *) original_header;
    struct eth_hdr *reh = (struct eth_hdr *) raddr;

    // set src / dst of eth header
    memcpy(&reh->src, &oeh->dst, 6);
    memcpy(&reh->dst, &oeh->src, 6);
    reh->type = oeh->type;

    // set ip-header
    struct ip_hdr *oih = (struct ip_hdr *) ((char *) oeh + ETH_HLEN);
    struct ip_hdr *rih = (struct ip_hdr *) ((char *) reh + ETH_HLEN);

    IPH_VHL_SET(rih, 4, 5);
    rih->tos = oih->tos;
    rih->len = oih->len;
    rih->id = oih->id;
    rih->offset = htons(0x4000);
    rih->ttl = oih->ttl;
    rih->proto = oih->proto;
    rih->src = oih->dest;
    rih->dest = oih->src;

    // set UDP-data
    struct udp_hdr *ouh = (struct udp_hdr *) ((char *) oih + IP_HLEN);
    struct udp_hdr *ruh = (struct udp_hdr *) ((char *) rih + IP_HLEN);
    assert(ouh == h);
    ruh->src = ouh->dest;
    ruh->dest = ouh->src;
    ruh->len = ouh->len;
    memcpy((char *) ruh + UDP_HLEN, (char *) ouh + UDP_HLEN, ntohs(ouh->len) - UDP_HLEN);

    // NOTE: it appears, the only way to get this to work is by not setting the checksum
    ruh->chksum = 0;

    rih->chksum = 0;
    rih->chksum = inet_checksum(rih, IP_HLEN);

    repl.valid_length = ETH_HLEN + htons(rih->len);

    dmb();

    UDP_DEBUG("=========== SENDING REPLYYY\n");
    err = enqueue_buf(st->send_qstate, &repl);

    return err;
}

static errval_t handle_UDP(struct enet_queue* q, struct devq_buf* buf,
                           struct udp_hdr *h, struct enet_driver_state* st,
                           lvaddr_t original_header) {
    errval_t err = SYS_ERR_OK;
    UDP_DEBUG("handling UDP packet\n");
    uint16_t d_p = ntohs(h->dest);
    if (d_p == UDP_ECHO_PORT) {  // TODO: outsource
        UDP_DEBUG("calling UDP-echo server\n");
        return udp_echo(q, buf, h, st, original_header);
    }

    // check if it belongs to an existing socket
    struct aos_udp_socket *socket = get_socket_from_port(st, d_p);
    if (socket == NULL) {
        UDP_DEBUG("no listening socket found\n");
        return err;
    }

    char *payload = (char *) h + UDP_HLEN;
    err = udp_socket_append_message(socket, (void *) payload, ntohs(h->len) - UDP_HLEN);

    return err;
}

// TODO: error-handling, checksum
errval_t handle_IP(struct enet_queue* q, struct devq_buf* buf,
                   lvaddr_t vaddr, struct enet_driver_state* st) {
    errval_t err;
    IP_DEBUG("handling IP packet\n");
    struct ip_hdr *header = (struct ip_hdr*) ((char *) vaddr + ETH_HLEN);
    print_ip_packet(header);

    IP_DEBUG("prot %d, %d, %d\n", header->proto, IP_PROTO_UDPLITE, IP_PROTO_UDP);
    switch (header->proto) {
    case IP_PROTO_ICMP:
        ICMP_DEBUG("got ICMP packet\n");
        struct icmp_echo_hdr *eh = (struct icmp_echo_hdr *)
            ((char *) header + IP_HLEN);
        err = handle_ICMP(q, buf, eh, st, vaddr);
        break;
    case IP_PROTO_UDPLITE:
    case IP_PROTO_UDP:
        UDP_DEBUG("handling UDP packet\n");
        struct udp_hdr *uh = (struct udp_hdr *)
            ((char *) header + IP_HLEN);
        err = handle_UDP(q, buf, uh, st, vaddr);
        break;
    case IP_PROTO_IGMP:
    case IP_PROTO_TCP:
    default:
        return LIB_ERR_NOT_IMPLEMENTED;
    }

    return err;
}

errval_t handle_packet(struct enet_queue* q, struct devq_buf* buf,
                       struct enet_driver_state* st) {
    ENET_DEBUG("handling new packet\n");
    /* print_packet(q, buf); */
    struct region_entry *entry = get_region(q, buf->rid);
    lvaddr_t vaddr = (lvaddr_t) entry->mem.vbase + buf->offset + buf->valid_data;

    struct eth_hdr *header = (struct eth_hdr*) vaddr;

    switch (htons(ETH_TYPE(header))) {
    case ETH_TYPE_ARP:
        ETHARP_DEBUG("Got an ARP packet!\n");
        handle_ARP(q, buf, vaddr, st);
        break;
    case ETH_TYPE_IP:
        IP_DEBUG("Got an IP packet!\n");
        handle_IP(q, buf, vaddr, st);
        break;
    default:
        ENET_DEBUG("Packet of unknown type!\n");
        break;
    }

    return SYS_ERR_OK;
}
