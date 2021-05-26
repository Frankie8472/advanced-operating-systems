#include <stdint.h>
#include <stdlib.h>
#include <aos/nameserver.h>
#include <aos/udp_service.h>

/**
 * \brief initialize an aos-socket with the given configurations.
 * \param sockref reference to socket instance data - all flags will be overwritten
 */
errval_t aos_socket_initialize(struct aos_socket *sockref,
                               uint32_t ip_dest, uint16_t f_port, uint16_t l_port) {
    errval_t err;
    err = nameservice_lookup(ENET_SERVICE_NAME, &sockref->_nschan);
    if (err_is_fail(err)) {
        return err;
    }

    size_t msgsize =
        sizeof(struct udp_service_message) +
        sizeof(struct udp_socket_create_info);

    struct udp_service_message *usm = malloc(msgsize);
    usm->type = CREATE;
    usm->port = l_port;
    usm->len = 0;
    struct udp_socket_create_info *usci = (struct udp_socket_create_info *) usm->data;
    usci->f_port = f_port;
    usci->ip_dest = ip_dest;

    void *response;
    size_t response_bytes;

    err = nameservice_rpc(sockref->_nschan, (void *) usm, msgsize,
                          &response, &response_bytes,
                          NULL_CAP, NULL_CAP);

    if (err_is_fail(err)) {
        return err;
    }

    sockref->ip_dest = ip_dest;
    sockref->f_port = f_port;
    sockref->l_port = l_port;

    return err;
}

/**
 * \brief send data over an aos_socket
 */
errval_t aos_socket_send(struct aos_socket *sockref, void *data, uint16_t len) {
    size_t msgsize = sizeof(struct udp_service_message) + len * sizeof(char);
    struct udp_service_message *usm = malloc(msgsize);

    usm->type = SEND;
    usm->port = sockref->l_port;
    usm->len = len;
    memcpy((void *) usm->data, data, len);


    void *response;
    size_t response_bytes;

    errval_t err = nameservice_rpc(sockref->_nschan, (void *) usm, msgsize,
                                   &response, &response_bytes,
                                   NULL_CAP, NULL_CAP);

    if (err_is_fail(err)) {
        return err;
    }

    return (errval_t) response;
}

errval_t aos_socket_send_to(struct aos_socket *sockref, void *data, uint16_t len,
                            uint32_t ip, uint16_t port) {
    size_t msgsize = sizeof(struct udp_service_message) + len * sizeof(char);
    struct udp_service_message *usm = malloc(msgsize);

    usm->type = SEND_TO;
    usm->port = sockref->l_port;
    usm->len = len;
    usm->ip = ip;
    usm->tgt_port = port;
    memcpy((void *) usm->data, data, len);

    void *response;
    size_t response_bytes;

    errval_t err = nameservice_rpc(sockref->_nschan, (void *) usm, msgsize,
                                   &response, &response_bytes,
                                   NULL_CAP, NULL_CAP);

    if (err_is_fail(err)) {
        return err;
    }

    return (errval_t) response;
}

struct udp_msg *aos_socket_receive(struct aos_socket *sockref) {
    size_t msgsize = sizeof(struct udp_service_message);
    struct udp_service_message *usm = malloc(msgsize);

    usm->type = RECV;
    usm->port = sockref->l_port;
    usm->len = 0;

    void *response;
    size_t response_bites;

    errval_t err = nameservice_rpc(sockref->_nschan, (void *) usm, msgsize,
                                   &response, &response_bites,
                                   NULL_CAP, NULL_CAP);

    if (err_is_fail(err)) {
        return NULL;
    }

    return response;
}
