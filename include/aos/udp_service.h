enum udp_service_messagetype {
    RECV,
    SEND,
    CREATE,
    DESTROY
};

struct udp_socket_create_info {
    uint32_t ip_dest;
    uint16_t f_port;
};

struct udp_service_message {
    enum udp_service_messagetype type;
    uint16_t port;
    uint16_t len;
    char data[];
};
