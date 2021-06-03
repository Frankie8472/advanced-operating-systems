#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <aos/paging.h>
#include <aos/nameserver.h>


#include <aos/waitset.h>
#include <aos/default_interfaces.h>
#define PANIC_IF_FAIL(err, msg)    \
    if (err_is_fail(err)) {        \
        USER_PANIC_ERR(err, msg);  \
    }

// #define SERVICE_NAME "/server"
// #define TEST_BINARY  "nameservicetest"

// extern struct aos_rpc fresh_connection;
static char *myresponse = "reply!!";
// static char buffer[512];
static void server_recv_handler(void *st, void *message,
                                size_t bytes,
                                void **response, size_t *response_bytes,
                                struct capref rx_cap, struct capref *tx_cap)
{
    // debug_printf("server: got a request: %s\n", (char *)message);
    *response = myresponse;
    *response_bytes = strlen(myresponse);
}



int main(int argc, char *argv[])
{
    

    errval_t err;
    if(argc != 2){
        return 1;
    }
    err = nameservice_register_properties(argv[1],server_recv_handler,NULL,false,"type=default");
    PANIC_IF_FAIL(err, "failed to register...\n");




    domainid_t client_pid;
    char buffer[1024];
    strcpy(buffer,"client_perf ");
    strcat(buffer,argv[1]);
    err = aos_rpc_process_spawn(get_init_rpc(),buffer,2,&client_pid);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to spawn client!\n");
    }

    struct waitset *default_ws = get_default_waitset();
    while (true) {
        err = event_dispatch(default_ws);
        // err = event_dispatch_non_block(get_default_waitset());
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }

    return EXIT_SUCCESS;
}
