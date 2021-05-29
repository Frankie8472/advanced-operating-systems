/*
 * Copyright (c) 2019, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstrasse 6, CH-8092 Zurich. Attn: Systems Group.
 */

#include <collections/hash_table.h>

#ifndef ENET_H_
#define ENET_H_


// #define ENET_DEBUG_OPTION 1

#if defined(ENET_DEBUG_OPTION)
#define ENET_DEBUG(x...) debug_printf("[enet] " x);
// print message, then the provided ip in readable format
void inline ENET_DEBUG_UI32_AS_IP(char *msg, uint32_t ip) {
    uint8_t *ref = (uint8_t *) &ip;
    ENET_DEBUG("%s %u.%u.%u.%u\n", msg, ref[3], ref[2], ref[1], ref[0]);
}
#else
#define ENET_DEBUG(fmt, ...) ((void)0)
void inline ENET_DEBUG_UI32_AS_IP(char *msg, uint32_t ip) {}
#endif

#define STATIC_ENET_IP 0x0a000201  // ip address of enet interface -> 10.0.2.1
#define UDP_ECHO_PORT 2521  // TODO: use morse-code here

#define ENET_PROMISC

#define TX_RING_SIZE 512
#define ENET_RX_FRSIZE 2048
#define ENET_RX_PAGES 256

#define ENET_MAX_PKT_SIZE 1536
#define ENET_MAX_BUF_SIZE 2048

#define RX_RING_SIZE (BASE_PAGE_SIZE / ENET_RX_FRSIZE) * ENET_RX_PAGES


#define ENET_RX_EMPTY 0x8000
#define ENET_SC_WRAP ((ushort)0x2000)
#define ENET_RX_intr ((ushort)0x1000)
#define ENET_RX_LAST ((ushort) 0x0800)
#define ENET_RX_FIRST ((ushort) 0x0400)
#define ENET_RX_MISS ((ushort) 0x0100)
#define ENET_RX_LG ((ushort) 0x0020)
#define ENET_RX_NO ((ushort) 0x0010)
#define ENET_RX_SH ((ushort) 0x0008)
#define ENET_RX_CR ((ushort) 0x0004)
#define ENET_RX_OV ((ushort) 0x0002)
#define ENET_RX_CL ((ushort) 0x0001)
#define ENET_RX_STATS ((ushort) 0x013f)

#define ENET_TX_READY 0x8000
#define ENET_TX_WRAP 0x2000
#define ENET_TX_LAST 0x0800
#define ENET_TX_CRC 0x0400

struct region_entry {
    uint32_t rid;
    struct dmem mem;
    struct region_entry* next;
};

struct enet_queue {
    struct devq q;
    size_t size;

    // stop and wake threashold
    uint16_t stop_th; 
    uint16_t wake_th;
    char* tso_hdr;


    struct capref regs;
    struct dmem desc_mem;
    enet_t* d;

    // hd + tail
    size_t head;
    size_t tail;

    // alignment
    size_t align;

    // Descriptor + Cleanq
    enet_bufdesc_array_t *ring;
    struct devq_buf *ring_bufs;

    struct region_entry* regions;
};

// struct to represent a single udp packet in a receive-buffer
struct udp_recv_elem {
    void *data;
    uint16_t f_port;
    uint32_t ip_addr;
    uint16_t len;
    struct udp_recv_elem *next;
};

// NOTE: all numbers (ip, ports,...) are in host-byte-order
struct aos_udp_socket {
    uint32_t ip_dest;
    uint16_t f_port;  // foreign port
    uint16_t l_port;  // local port
    /* uint8_t listen_only;  // non-zero if socket is only for listening */

    struct udp_recv_elem *receive_buffer;
    struct udp_recv_elem *last_elem;
    /* uint64_t sock_id; */

    struct aos_udp_socket* next;
};

struct aos_icmp_socket {
    uint32_t ip;
    uint16_t id_mask;  // for sending: id = seqno ^ mask
    uint16_t seq_sent;
    uint16_t seq_rcv;

    struct aos_icmp_socket *next;
};

struct enet_driver_state {
    struct bfdriver_instance *bfi;
    struct capref regs;
    lvaddr_t d_vaddr;

    struct enet_queue* rxq;  // receive queue
    struct enet_queue* txq;  // send queue
    enet_t* d;
    uint64_t mac;

    uint32_t phy_id;

    struct capref rx_mem;  // receive memcap
    struct capref tx_mem;  // send memcap

    collections_hash_table* arp_table;  // arp-related state
    collections_hash_table* inv_table;  // inverse arp-table: ip 2 mac
    struct enet_qstate* send_qstate;  // regionman for send-queue

    struct aos_udp_socket *sockets;  // TODO: hash-set
    struct aos_icmp_socket *pings;
};

// ETH handler functions
void print_arp_table(struct enet_driver_state *st);
errval_t handle_ARP(struct enet_queue* q, struct devq_buf* buf,
                    lvaddr_t vaddr, struct enet_driver_state* st);
errval_t handle_IP(struct enet_queue* q, struct devq_buf* buf,
                   lvaddr_t vaddr, struct enet_driver_state* st);
errval_t handle_packet(struct enet_queue* q, struct devq_buf* buf,
                       struct enet_driver_state* st);

// UDP Socket functions
/* struct aos_udp_socket* get_socket_from_id(struct enet_driver_state *st, */
/*                                           uint64_t socket_id); */
#define UDP_SOCK_MAX_LEN (2048 - UDP_HLEN - IP_HLEN - ETH_HLEN)

struct aos_udp_socket* get_socket_from_port(struct enet_driver_state *st,
                                            uint16_t port);
errval_t udp_socket_append_message(struct aos_udp_socket *s, uint16_t f_port, uint32_t ip,
                                   void *data, uint32_t len);
struct udp_recv_elem *udp_socket_receive(struct aos_udp_socket *s);
errval_t udp_socket_teardown(struct enet_driver_state *st,
                             struct aos_udp_socket *socket);
struct aos_udp_socket* create_udp_socket(struct enet_driver_state *st,
                                         uint32_t ip_dest, uint16_t f_port,
                                         uint16_t l_port);
errval_t arp_request(struct enet_driver_state *st, uint32_t ip_to);
errval_t udp_socket_send(struct enet_driver_state *st, uint16_t port,
                         void *data, uint16_t len);
errval_t udp_socket_send_to(struct enet_driver_state *st, uint16_t port,
                            void *data, uint16_t len, uint32_t ip_to,
                            uint16_t port_to);
struct aos_icmp_socket *get_ping_socket(struct enet_driver_state *st, uint32_t ip);
struct aos_icmp_socket *create_ping_socket(struct enet_driver_state *st, uint32_t ip);
errval_t ping_socket_teardown(struct enet_driver_state *st, uint32_t ip);
errval_t ping_socket_send_next(struct enet_driver_state *st, uint32_t ip);
uint16_t ping_socket_get_acked(struct enet_driver_state *st, uint32_t ip);

// service handler
void name_server_initialize(struct enet_driver_state *st);

#define ENET_HASH_BITS 6
#define ENET_CRC32_POLY 0xEDB88320

#endif // ndef ENET_H_
