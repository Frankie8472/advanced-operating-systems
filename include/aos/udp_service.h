#define ENET_SERVICE_NAME "/ethernet"

enum udp_service_messagetype {
    RECV,
    SEND,
    SEND_TO,
    CREATE,
    DESTROY,
    ARP_TBL,
    ICMP_PING_SEND,
    ICMP_PING_RECV
};

struct udp_socket_create_info {
    uint16_t f_port;
    uint32_t ip_dest;
};

struct udp_service_message {
    enum udp_service_messagetype type;
    uint16_t port;
    uint16_t len;
    uint16_t tgt_port;
    uint32_t ip;
    char data[0];
} __attribute__((__packed__));

struct aos_socket {
    uint16_t f_port;  // foreign port
    uint16_t l_port;  // local port
    uint32_t ip_dest;
    nameservice_chan_t _nschan;
};

struct aos_ping_socket {
    uint32_t ip;
    nameservice_chan_t _nschan;
};

struct udp_msg {
    uint16_t f_port;
    uint16_t len;
    uint32_t ip;
    char data[0];
} __attribute__((__packed__));

errval_t aos_socket_initialize(struct aos_socket *sockref, uint32_t ip_dest, uint16_t f_port, uint16_t l_port);

errval_t aos_socket_send(struct aos_socket *sockref, void *data, uint16_t len);

errval_t aos_socket_send_to(struct aos_socket *sockref, void *data, uint16_t len,
                            uint32_t ip, uint16_t port);

errval_t aos_socket_receive(struct aos_socket *sockref, struct udp_msg *retptr);

errval_t aos_socket_teardown(struct aos_socket *sockref);

void aos_arp_table_get(char *rtptr);

errval_t aos_ping_init(struct aos_ping_socket *s, uint32_t ip);

errval_t aos_ping_send(struct aos_ping_socket *s);

uint16_t aos_ping_recv(struct aos_ping_socket *s);
