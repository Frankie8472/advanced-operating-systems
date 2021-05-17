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

// #define SERVICE_NAME "myservicename"
// #define TEST_BINARY  "nameservicetest"

// // extern struct aos_rpc fresh_connection;
// static char *myresponse = "reply!!";

// static void server_recv_handler(void *st, void *message,
//                                 size_t bytes,
//                                 void **response, size_t *response_bytes,
//                                 struct capref rx_cap, struct capref *tx_cap)
// {
//     debug_printf("server: got a request: %s\n", (char *)message);
//     *response = myresponse;
//     *response_bytes = strlen(myresponse);
// }



int main(int argc, char *argv[])
{
    

    errval_t err;
    debug_printf("Server\n");
    
    char * name;
    aos_rpc_process_get_name(aos_rpc_get_process_channel(),0,&name);
    debug_printf("Revecived name %s\n",name);

    coreid_t core;
    err = aos_rpc_call(get_ns_rpc(),NS_GET_PROC_CORE,0,&core);
    debug_printf("Revecived core %d\n",core);

    domainid_t * pids;
    size_t pid_count;
    err = aos_rpc_process_get_all_pids(aos_rpc_get_process_channel(),&pids,&pid_count);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"failed to list all pids");
    }   

    for(size_t i = 0; i < pid_count;++i){
        debug_printf("Pid %d\n",pids[i]);
    }



    
    // debug_printf("register with nameservice '%s'\n", SERVICE_NAME);
    // debug_printf("0x%lx\n", get_init_rpc());
    // err = nameservice_register_properties(SERVICE_NAME, server_recv_handler, NULL,false);
    // PANIC_IF_FAIL(err, "failed to register...\n");
    domainid_t my_pid;

    err = aos_rpc_process_spawn(get_init_rpc(),"client",0,&my_pid);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to spawn client!\n");
    }
    
    


    

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
