#define ENET_SERVICE_NAME "/ethernet"

enum udp_service_messagetype {
    RECV,
    SEND,
    CREATE,
    DESTROY
};

struct udp_socket_create_info {
    uint16_t f_port;
    uint32_t ip_dest;
};

struct udp_service_message {
    enum udp_service_messagetype type;
    uint16_t port;
    uint16_t len;
    char data[0];
};

struct aos_socket {
    uint16_t f_port;  // foreign port
    uint16_t l_port;  // local port
    uint32_t ip_dest;
    nameservice_chan_t _nschan;
};

errval_t aos_socket_initialize(struct aos_socket *sockref, uint32_t ip_dest, uint16_t f_port, uint16_t l_port);

errval_t aos_socket_send(struct aos_socket *sockref, void *data, uint16_t len);

void *aos_socket_receive(struct aos_socket *sockref, size_t *len);
