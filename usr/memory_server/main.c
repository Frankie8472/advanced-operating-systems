#include <stdio.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>


static void handle_mem_server_request(struct aos_rpc *r, struct capref client_cap, struct capref * server_cap){

    debug_printf("Handling mem server binding request\n");
    errval_t err;
    struct lmp_endpoint * lmp_ep;
    struct capref self_ep_cap = (struct capref) {
      .cnode = cnode_task,
      .slot = TASKCN_SLOT_SELFEP
    };


    // assert()

    err = endpoint_create(256,&self_ep_cap,&lmp_ep);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to create ep in memory server\n");
    }

    // ON_ERR_RETURN(err,LIB_ERR_ENDPOINT_CREATE);
    static struct aos_rpc rpc;
    err = aos_rpc_init(&rpc);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to init rpc in memory server\n");
    }


    err = aos_rpc_init_lmp_without_init(&rpc,self_ep_cap,client_cap,lmp_ep);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to register waitset on rpc\n");
    }


    *server_cap = self_ep_cap;
}


int main(int argc, char *argv[])
{   

    errval_t err;
    struct aos_rpc * init_rpc = get_init_rpc();

    err = aos_rpc_call(init_rpc,AOS_RPC_SERVICE_ON,1);


    aos_rpc_register_handler(init_rpc, AOS_RPC_MEM_SERVER_REQ, &handle_mem_server_request);
    debug_printf("Message handler loop\n");
    struct waitset *default_ws = get_default_waitset();
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            debug_printf("err in event_dispatch\n");
            abort();
        }
    }


    return 0;
}
