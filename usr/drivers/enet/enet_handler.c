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

#include <collections/hash_table.h>

#include "enet.h"
#include "enet_regionman.h"
#include "enet_handler.h"

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

static void print_arp_packet(struct arp_hdr* h) {
    ENET_DEBUG("======== ARP PACKET ========\n");
    ENET_DEBUG("hwtype - %d\n", ntohs(h->hwtype));
    ENET_DEBUG("proto - %d\n", ntohs(h->proto));
    ENET_DEBUG("hwlen - %d\n", h->hwlen);
    ENET_DEBUG("protolen - %d\n", h->protolen);
    ENET_DEBUG("opcode - %d\n", ntohs(h->opcode));
    ENET_DEBUG("ip_src - %d\n", ntohs(h->ip_src));
    ENET_DEBUG("ip_dst - %d\n", ntohs(h->ip_dst));
}

errval_t handle_ARP(struct enet_queue* q, struct devq_buf* buf,
                    lvaddr_t vaddr) {
    struct arp_hdr *header = (struct arp_hdr*) ((char *)vaddr + ETH_HLEN);
    print_arp_packet(header);
    return SYS_ERR_OK;
}

errval_t handle_packet(struct enet_queue* q, struct devq_buf* buf) {
    ENET_DEBUG("handling new packet\n");
    /* print_packet(q, buf); */
    struct region_entry *entry = get_region(q, buf->rid);
    lvaddr_t vaddr = (lvaddr_t) entry->mem.vbase + buf->offset + buf->valid_data;

    struct eth_hdr *header = (struct eth_hdr*) vaddr;

    switch (htons(ETH_TYPE(header))) {
    case ETH_TYPE_ARP:
        ENET_DEBUG("Got an ARP packet!\n");
        handle_ARP(q, buf, vaddr);
        break;
    case ETH_TYPE_IP:
        ENET_DEBUG("Got an IP packet!\n");
        break;
    default:
        ENET_DEBUG("Packet of unknown type!\n");
        break;
    }

    return SYS_ERR_OK;
}
