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
#include <netutil/htons.h>

#include <collections/hash_table.h>

#include "enet.h"
#include "enet_regionman.h"
#include "enet_handler.h"

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
    ENET_DEBUG("======== IP  PACKET ========\n");
    ENET_DEBUG("version - %d\n", ih->v_hl & 0x0f);
    ENET_DEBUG("header length - %d\n", ih->v_hl & 0xf0);
    // TODO: tos
    ENET_DEBUG("length - %d\n", ntohs(ih->len));
    ENET_DEBUG("id - %d\n", ntohs(ih->id));
    ENET_DEBUG("offset - %d\n", ntohs(ih->offset));
    ENET_DEBUG("ttl - %d\n", ih->ttl);
    ENET_DEBUG("protocol - %d\n", ih->proto);
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

// TODO: make more concise with architecture in mind
static inline uint64_t eth_addr_to_u64(struct eth_addr *a) {
    return (0  // to make it look more aligned
            | a->addr[0] << 7
            | a->addr[1] << 6
            | a->addr[2] << 5
            | a->addr[3] << 4
            | a->addr[4] << 3
            | a->addr[5] << 2);
}

/**
 * \brief convert an uint64_t to an eth address:
 * converts src, writes it into dest
 */
// TODO: make more concise with architecture in mind
static inline void u64_to_eth_addr(uint64_t src, struct eth_addr* dest) {
    dest->addr[0] = src >> 7;
    dest->addr[1] = src >> 6;
    dest->addr[2] = src >> 5;
    dest->addr[3] = src >> 4;
    dest->addr[4] = src >> 3;
    dest->addr[5] = src >> 2;
}

static errval_t arp_request_handle(struct enet_queue* q, struct devq_buf* buf,
                                   struct arp_hdr *h, struct enet_driver_state* st,
                                   lvaddr_t original_header) {
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

    // possibly reply to it
    if (ntohl(h->ip_dst) == STATIC_ENET_IP) {
        ETHARP_DEBUG("is for me?\n");
        ETHARP_DEBUG("(o--o)\n");
        ETHARP_DEBUG("|    |\n");
        ETHARP_DEBUG("`-><-'\n");

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

        ENET_DEBUG("=========== SENDING REPLYYY\n");
        enqueue_buf(st->send_qstate, &repl);
    }

    // TODO: prepare and send response
    return SYS_ERR_OK;
}

errval_t handle_ARP(struct enet_queue* q, struct devq_buf* buf,
                    lvaddr_t vaddr, struct enet_driver_state* st) {
    struct arp_hdr *header = (struct arp_hdr*) ((char *)vaddr + ETH_HLEN);
    print_arp_packet(header);

    switch (ntohs(header->opcode)) {
    case ARP_OP_REQ:
        ENET_DEBUG("ARP Request\n");
        arp_request_handle(q, buf, header, st, vaddr);
        break;
    case ARP_OP_REP:
        ENET_DEBUG("ARP Reply\n");
        // TODO
        break;
    default:
        ENET_DEBUG("Unknown ARP opcode\n");
        // TODO: add errorcode to return here maybe?
        break;
    }
    return SYS_ERR_OK;
}

errval_t handle_IP(struct enet_queue* q, struct devq_buf* buf,
                   lvaddr_t vaddr, struct enet_driver_state* st) {
    return SYS_ERR_OK;
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
        ENET_DEBUG("Got an ARP packet!\n");
        handle_ARP(q, buf, vaddr, st);
        break;
    case ETH_TYPE_IP:
        ENET_DEBUG("Got an IP packet!\n");
        handle_IP(q, buf, vaddr, st);
        break;
    default:
        ENET_DEBUG("Packet of unknown type!\n");
        break;
    }

    return SYS_ERR_OK;
}
