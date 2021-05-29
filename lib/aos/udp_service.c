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
    errval_t *erref = malloc(sizeof(errval_t));
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

    void *response = (void *) erref;
    size_t response_bytes;

    err = nameservice_rpc(sockref->_nschan, (void *) usm, msgsize,
                          &response, &response_bytes,
                          NULL_CAP, NULL_CAP);
    free(usm);

    sockref->ip_dest = ip_dest;
    sockref->f_port = f_port;
    sockref->l_port = l_port;

    if (!err_is_fail(err)) {
        err = *erref;
    }

    free(erref);
    return err;
}

/**
 * \brief send data over an aos_socket
 */
errval_t aos_socket_send(struct aos_socket *sockref, void *data, uint16_t len) {
    errval_t *erref = malloc(sizeof(errval_t));
    size_t msgsize = sizeof(struct udp_service_message) + len * sizeof(char);
    struct udp_service_message *usm = malloc(msgsize);

    usm->type = SEND;
    usm->port = sockref->l_port;
    usm->len = len;
    memcpy((void *) usm->data, data, len);


    void *response = (void *) erref;
    size_t response_bytes;

    errval_t err = nameservice_rpc(sockref->_nschan, (void *) usm, msgsize,
                                   &response, &response_bytes,
                                   NULL_CAP, NULL_CAP);
    free(usm);

    if (!err_is_fail(err)) {
        err = *erref;
    }

    free(erref);
    return err;
}

errval_t aos_socket_send_to(struct aos_socket *sockref, void *data, uint16_t len,
                            uint32_t ip, uint16_t port) {
    errval_t *erref = malloc(sizeof(errval_t));
    size_t msgsize = sizeof(struct udp_service_message) + len * sizeof(char);
    struct udp_service_message *usm = malloc(msgsize);

    usm->type = SEND_TO;
    usm->port = sockref->l_port;
    usm->len = len;
    usm->ip = ip;
    usm->tgt_port = port;
    memcpy((void *) usm->data, data, len);

    void *response = (void *) erref;
    size_t response_bytes;

    errval_t err = nameservice_rpc(sockref->_nschan, (void *) usm, msgsize,
                                   &response, &response_bytes,
                                   NULL_CAP, NULL_CAP);
    free(usm);

    if (!err_is_fail(err)) {
        err = *erref;
    }

    free(erref);
    return err;
}

errval_t aos_socket_receive(struct aos_socket *sockref, struct udp_msg *retptr) {
    size_t msgsize = sizeof(struct udp_service_message);
    struct udp_service_message *usm = malloc(msgsize);

    usm->type = RECV;
    usm->port = sockref->l_port;
    usm->len = 0;

    void *response = retptr;
    size_t response_bites;

    errval_t err = nameservice_rpc(sockref->_nschan, (void *) usm, msgsize,
                                   &response, &response_bites,
                                   NULL_CAP, NULL_CAP);
    free(usm);

    if (err_is_fail(err) || response_bites == 0) {
        return LIB_ERR_NOT_IMPLEMENTED;
    }

    return SYS_ERR_OK;
}

errval_t aos_socket_teardown(struct aos_socket *sockref) {
    errval_t *erref = malloc(sizeof(errval_t));
    size_t msgsize = sizeof(struct udp_service_message);
    struct udp_service_message *usm = malloc(msgsize);

    usm->type = DESTROY;
    usm->port = sockref->l_port;

    void *response = erref;
    size_t response_botes;

    errval_t err = nameservice_rpc(sockref->_nschan, (void *) usm, msgsize,
                                   &response, &response_botes,
                                   NULL_CAP, NULL_CAP);
    free(usm);

    err = err_is_fail(err) ? err : *erref;

    free(erref);
    return err;
}

/**
 * \brief retrieve a string describing the device's current arp-table
 */
void aos_arp_table_get(char *rtptr) {
    errval_t err;
    struct udp_service_message *usm = malloc(sizeof(struct udp_service_message));
    usm->type = ARP_TBL;

    nameservice_chan_t _nschan;
    err = nameservice_lookup(ENET_SERVICE_NAME, &_nschan);

    void *response = (void *) rtptr;
    size_t response_butes;

    err = nameservice_rpc(_nschan, (void *) usm, strlen((char *) usm),
                                   &response, &response_butes,
                                   NULL_CAP, NULL_CAP);
    free(usm);

    if (err_is_fail(err)) {
        DEBUG_ERR(err, "failed to do the nameservice rpc\n");
    }
}

errval_t aos_ping_init(struct aos_ping_socket *s, uint32_t ip) {
    s->ip = ip;
    return nameservice_lookup(ENET_SERVICE_NAME, &s->_nschan);
}

errval_t aos_ping_send(struct aos_ping_socket *s) {
    errval_t *erref = malloc(sizeof(errval_t));
    size_t msgsize = sizeof(struct udp_service_message);
    struct udp_service_message *usm = malloc(msgsize);

    usm->type = ICMP_PING_SEND;
    usm->ip = s->ip;

    void *response = erref;
    size_t response_betes;

    errval_t err = nameservice_rpc(s->_nschan, (void *) usm, msgsize,
                                   &response, &response_betes,
                                   NULL_CAP, NULL_CAP);
    free(usm);
    err = err_is_fail(err) ? err : *erref;
    free(erref);
    return LIB_ERR_NOT_IMPLEMENTED;
}

uint16_t aos_ping_recv(struct aos_ping_socket *s) {
    uint16_t *resref = malloc(sizeof(uint16_t));
    size_t msgsize = sizeof(struct udp_service_message);
    struct udp_service_message *usm = malloc(msgsize);

    usm->type = ICMP_PING_RECV;
    usm->ip = s->ip;

    void *response = resref;
    size_t response_betes;

    errval_t err = nameservice_rpc(s->_nschan, (void *) usm, msgsize,
                                   &response, &response_betes,
                                   NULL_CAP, NULL_CAP);
    free(usm);
    uint16_t res = err_is_fail(err) ? 0 : *resref;
    free(resref);
    return res;
}
