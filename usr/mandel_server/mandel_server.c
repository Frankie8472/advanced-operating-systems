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

static void server_recv_handler(void *st, void *message,
                                size_t bytes,
                                void **response, size_t *response_bytes,
                                struct capref rx_cap, struct capref *tx_cap)
{
    if (strcmp(message, "set_connection") == 0) {
        connection_frame = rx_cap;
        static const char resp[] = "OK";
        *response = resp;
        *response_bytes = sizeof resp;
    }
    else if (strcmp(message, "set_ipi") == 0) {
        ipi_notifier = rx_cap;
        static const char resp[] = "OK";
        *response = resp;
        *response_bytes = sizeof resp;
    }
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
