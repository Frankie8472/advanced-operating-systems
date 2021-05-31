#include <stdio.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <aos/paging.h>
#include <aos/nameserver.h>
#define PANIC_IF_FAIL(err, msg)    \
    if (err_is_fail(err)) {        \
        USER_PANIC_ERR(err, msg);  \
    }

#define CALCULATE_REQUEST 1


#define SERVICE_NAME "/myservicenamea"
#define TEST_BINARY  "nameservicetest"

static char *myrequest = "request !!";

int main(int argc, char *argv[])
{
    
    // printf("Hello, world! from userspace\n");
    // printf("%s\n", argv[1]);
    // stack_overflow();

    errval_t err;
    debug_printf("Client\n");


    

    /* look up service using name server */
    nameservice_chan_t chan;
    // err = nameservice_lookup(SERVICE_NAME, &chan);
    err = nameservice_lookup_with_prop("/","type=ethernet",&chan);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to lookup service!\n");
    }

    debug_printf("Got the service %p. Sending request '%s'\n", chan, myrequest);

    void *request = myrequest;
    size_t request_size = strlen(myrequest);

    void *response;
    size_t response_bytes;
    err = nameservice_rpc(chan, request, request_size,
                          &response, &response_bytes,
                          NULL_CAP, NULL_CAP);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"failed to do the nameservice rpc\n");
    }

    debug_printf("got response: %s\n", (char *)response);

    size_t num;
    char * ret_string[512];
    err = nameservice_enumerate("/",&num,(char**)ret_string);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to enumerate!\n");
    }
    debug_printf("Got response : %d\n",num);
    for(int i = 0; i < num;++i){
        debug_printf("[%d] = %s\n",i,ret_string[i]);
    }

    // char * name;
    // err = aos_rpc_process_get_name(aos_rpc_get_process_channel(),1,&name);

    // char buffer[512];

    err = nameservice_enumerate_with_props("/","type=ethernet",&num,(char**)ret_string);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to enumerate!\n");
    }
    debug_printf("Got response : %d\n",num);
    for(int i = 0; i < num;++i){
        debug_printf("[%d] = %s\n",i,ret_string[i]);
    }




    char* props;
    err = nameservice_get_props(SERVICE_NAME,&props);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to get props!\n");
    }
    debug_printf("\n%s\n",props);
    //   domainid_t * pids;
    // size_t pid_size;
    // err = aos_rpc_process_get_all_pids(aos_rpc_get_process_channel(),&pids,&pid_size);

    // for(int i = 0; i < pid_size; ++i){
    //     debug_printf("%d\n",pids[i]);
    // }    


    /*struct aos_rpc server_rpc;
    do {
        err = aos_rpc_new_binding_by_name("server", &server_rpc);
        aos_rpc_initialize_binding(&server_rpc, CALCULATE_REQUEST, 1, 1, AOS_RPC_WORD, AOS_RPC_WORD);
    }while(err_is_fail(err));

    do {
        err = aos_rpc_new_binding_by_name("server", &server_rpc2);
        aos_rpc_initialize_binding(&server_rpc2, CALCULATE_REQUEST, 1, 1, AOS_RPC_WORD, AOS_RPC_WORD);
    }while(err_is_fail(err));
    
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"Failed to init binding iwth server rpc\n");
    // }


    debug_printf("sending bytes: %d\n", disp_get_domain_id());
    char arr[128];
    struct aos_rpc_varbytes bytes = {
        .bytes = arr,
        .length = sizeof arr
    };

    for (int i = 0; i < bytes.length; i++) {
        bytes.bytes[i] = i;
    }

    err = aos_rpc_call(&server_rpc, AOS_RPC_SEND_VARBYTES, bytes);
    // err = aos_rpc_send_number(&server_rpc,disp_get_domain_id());
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to send number from client to server\n");
    }*/


    //err = aos_rpc_init(&server_rpc);



    // debug_printf("Message handler loop\n");
    // struct waitset *default_ws = get_default_waitset();
    // while (true) {
    //     // debug_printf("sending number: %d\n",disp_get_domain_id());
    //     // err = aos_rpc_send_number(&server_rpc,disp_get_domain_id());
    //     // if(err_is_fail(err)){
    //     //     DEBUG_ERR(err,"Failed to send number from client to server\n");
    //     // }


    //     err = event_dispatch(default_ws);
    //     if (err_is_fail(err)) {
    //         DEBUG_ERR(err, "in event_dispatch");
    //         abort();
    //     }
    // }
    return EXIT_SUCCESS;
}
