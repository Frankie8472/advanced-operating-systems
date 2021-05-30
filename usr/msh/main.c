#include <stdio.h>
#include <stdint.h>

#include <aos/aos.h>
#include <aos/udp_service.h>
#include <aos/aos_datachan.h>
#include <aos/default_interfaces.h>

static char *welcome = "\n"
    " __  __   ____    _   _\n"
    "|  \\/  | / ___|  | | | |\n"
    "| |\\/| | \\___ \\  | |_| |\n"
    "| |  | |  ___) | |  _  |\n"
    "|_|  |_| |____/  |_| |_|\n";

static char josh_out_buf[1990];

static struct aos_datachan josh_in;
static struct aos_datachan josh_out;

static errval_t spawn_josh(void) {
    errval_t err;

    char tmpbuf[200];

    struct spawn_request_header *header =
        (struct spawn_request_header *) tmpbuf;
    header->argc = 1;
    header->envc = 0;

    coreid_t core = 0;


    struct spawn_request_arg *arg =
        (struct spawn_request_arg *) (tmpbuf + sizeof(struct spawn_request_header));
    arg->length = 5;
    strcpy(arg->str, "josh");

    struct aos_rpc_varbytes bytes = {
        .length = sizeof(*header) + sizeof(*arg) + 5,
        .bytes = (char *) header
    };

    domainid_t pid;

    struct capref rpc_frame;
    struct capref in_frame;
    struct capref out_frame;

    err = frame_alloc(&rpc_frame, BASE_PAGE_SIZE, NULL);
    err = frame_alloc(&in_frame, BASE_PAGE_SIZE, NULL);
    err = frame_alloc(&out_frame, BASE_PAGE_SIZE, NULL);

    err = aos_rpc_call(get_init_rpc(), INIT_IFACE_SPAWN_EXTENDED, bytes, core, rpc_frame, out_frame, in_frame, &pid);

    void *in_addr;
    err = paging_map_frame_complete(get_current_paging_state(), &in_addr,  in_frame, NULL, NULL);
    err = aos_dc_init_ump(&josh_in, 64, (lvaddr_t) in_addr, BASE_PAGE_SIZE, 0);


    void *out_addr;
    err = paging_map_frame_complete(get_current_paging_state(), &out_addr,  out_frame, NULL, NULL);
    err = aos_dc_init_ump(&josh_out, 64, (lvaddr_t) out_addr, BASE_PAGE_SIZE, 1);

    return err;
}

static void replace_newlines(char *msg, int len) {
    for (int i = 0; i < len; i++) {
        if (msg[i] == '\n') {
            msg[i] = 13;
        }
    }
}

static void flush_out(struct udp_msg *cl, struct aos_socket *sock) {
    while (aos_dc_can_receive(&josh_out)) {
        size_t rcvd;
        aos_dc_receive_available(&josh_out, 1990, josh_out_buf, &rcvd);

        aos_socket_send_to(sock, josh_out_buf, rcvd, cl->ip, cl->f_port);
        debug_printf("wohoo\n");
    }
}

static errval_t forward_in(struct udp_msg *cl, struct aos_socket *sock) {
    errval_t err = aos_socket_receive(sock, cl);
    if (err_is_fail(err)) {
        /* err = aos_socket_receive(&sock, in); */
        return err;
    }

    replace_newlines(cl->data, cl->len);
    /* cl->data[cl->len + 1] = 0; */
    /* printf("HERE\n"); */
    /* printf("<<< %s, %d, \n", cl->data, cl->len); */
    aos_dc_send(&josh_in, cl->len, cl->data);
    return err;
}

// command from machine: stty -icanon -echo && nc -u 10.0.2.1 1525
int main(int argc, char **argv) {
    if (argc < 2) {
        printf("ERROR: no port given\n");
        return EXIT_SUCCESS;
    }
    int port = atoi(argv[1]);
    printf("starting msh on port %d\n", port);

    struct aos_socket sock;
    __unused errval_t err = aos_socket_initialize(&sock, 0, 0, port);
    if (err_is_fail(err)) {
        printf("unable to initialize socket\n");
        return EXIT_SUCCESS;
    }

    printf("spawning josh\n");
    err = spawn_josh();

    /* return EXIT_SUCCESS; */

    // wait for first message
    struct udp_msg *in = malloc(sizeof(struct udp_msg) + 2048 * sizeof(char));
    do {
        err = aos_socket_receive(&sock, in);
        /* err = forward_in(in, &sock); */
    } while (err_is_fail(err));

    printf("received a connection!\n");
    /* forward_in(in, &sock); */
    aos_socket_send_to(&sock, welcome, strlen(welcome), in->ip, in->f_port);
    /* aos_socket_send_to(&sock, josh_out_buf, rcvd, in->ip, in->f_port); */
    flush_out(in, &sock);

    replace_newlines(in->data, in->len);
    printf("<<< %s, %d\n", in->data, in->len);
    aos_dc_send(&josh_in, in->len, in->data);
    /* flush_out(in, &sock); */
    /* aos_dc_send(&josh_in, in->len, in->data); */
    /* flush_out(in, &sock); */
    /* do { */
    /*     err = aos_socket_receive(&sock, in); */
    /*     /\* err = forward_in(in, &sock); *\/ */
    /* } while (err_is_fail(err)); */

    /* aos_dc_send(&josh_in, strlen("josh\n"), "josh\n"); */
    /* aos_dc_send(&josh_in, strlen("\n"), "\n"); */
    /* printf("first1: %s\n", in->data); */
    /* replace_newlines(in->data, in->len); */
    /* aos_dc_send(&josh_in, 1, "\13"); */
    /* printf("first: %s\n", in->data); */
    /* aos_dc_send(&josh_in, in->len, in->data); */
    // main event loop
    for (;;) {
        flush_out(in, &sock);

        forward_in(in, &sock);
    }

    return EXIT_SUCCESS;
}
