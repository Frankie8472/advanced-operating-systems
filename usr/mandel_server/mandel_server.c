#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <aos/paging.h>
#include <aos/nameserver.h>
#include <aos/waitset.h>
#include <aos/default_interfaces.h>

#include "calculate.h"


#define PANIC_IF_FAIL(err, msg)    \
    if (err_is_fail(err)) {        \
        USER_PANIC_ERR(err, msg);  \
    }

#define SERVICE_NAME "/mandel"
// #define TEST_BINARY  "nameservicetest"

struct capref connection_frame;
struct capref ipi_notifier;

struct aos_rpc calc_connection;


static void setup_calc_connection(struct capref frame)
{
    errval_t err;
    void *shared;
    char buf[128];
    debug_print_cap_at_capref(buf, 128, frame);
    debug_printf("cap is %s\n", buf);
    err = paging_map_frame_complete(get_current_paging_state(), &shared, frame, NULL, NULL);
    DEBUG_ERR(err, ":O");
    aos_rpc_init_ump_default(&calc_connection, (lvaddr_t) shared, get_phys_size(frame), 1);
    aos_rpc_set_interface(&calc_connection, get_dispatcher_interface(), DISP_IFACE_N_FUNCTIONS, malloc(DISP_IFACE_N_FUNCTIONS * sizeof(void *)));

    void handle_roundtrip(struct aos_rpc *rpc) { return; }
    aos_rpc_register_handler(&calc_connection, AOS_RPC_ROUNDTRIP, &handle_roundtrip);
}


static void remote_ipi(struct capref ipi_ep)
{
    ump_chan_switch_remote_pinged(&calc_connection.channel.ump, ipi_ep);
}

static void local_ipi(struct capref *ipi_ep)
{
    struct lmp_endpoint *ep;
    struct capref epcap;
    endpoint_create(LMP_RECV_LENGTH, &epcap, &ep);
    slot_alloc(ipi_ep);
    cap_retype(*ipi_ep, epcap, 0, ObjType_EndPointIPI, 0, 1);
    ump_chan_switch_local_pinged(&calc_connection.channel.ump, ep);
}


static void server_recv_handler(void *st, void *message,
                                size_t bytes,
                                void **response, size_t *response_bytes,
                                struct capref rx_cap, struct capref *tx_cap)
{
    if (strcmp(message, "set_connection") == 0) {
        setup_calc_connection(rx_cap);
    }
    else if (strcmp(message, "set_ipi") == 0) {
        remote_ipi(rx_cap);
        local_ipi(tx_cap);
    }

    static const char resp[] = "OK";
    *response = resp;
    *response_bytes = sizeof resp;
}



int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Please specify an identifier as an argument\n");
        return 1;
    }

    errval_t err;
    char buffer[64];
    strcpy(buffer, SERVICE_NAME);
    strcat(buffer, argv[1]);

    err = nameservice_register_properties(buffer, server_recv_handler, NULL, false, "type=mandel");

    PANIC_IF_FAIL(err, "failed to register...\n");
    
    debug_printf("Message handler loop\n");
    struct waitset *default_ws = get_default_waitset();
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

    return EXIT_SUCCESS;
}
