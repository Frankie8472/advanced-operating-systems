#include <stdio.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>





int main(int argc, char *argv[])
{
    
    // printf("Hello, world! from userspace\n");
    // printf("%s\n", argv[1]);
    // stack_overflow();

    errval_t err;
    // debug_printf("Client\n");


  



    // char * name;
    // err = aos_rpc_process_get_name(aos_rpc_get_process_channel(),1,&name);




    //   domainid_t * pids;
    // size_t pid_size;
    // err = aos_rpc_process_get_all_pids(aos_rpc_get_process_channel(),&pids,&pid_size);

    // for(int i = 0; i < pid_size; ++i){
    //     debug_printf("%d\n",pids[i]);
    // }    


    struct aos_rpc server_rpc;
    do {
        err = aos_rpc_new_binding_by_name("server",&server_rpc);
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
    }


    err = aos_rpc_init(&server_rpc);
    


    struct waitset *default_ws = get_default_waitset();
    while (true) {
        // debug_printf("sending number: %d\n",disp_get_domain_id());
        // err = aos_rpc_send_number(&server_rpc,disp_get_domain_id());
        // if(err_is_fail(err)){
        //     DEBUG_ERR(err,"Failed to send number from client to server\n");
        // }


        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
    return EXIT_SUCCESS;
}
