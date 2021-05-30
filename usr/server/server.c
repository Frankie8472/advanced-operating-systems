#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <aos/paging.h>
#include <aos/nameserver.h>

#define MAX_CONNS 16

#define CALCULATE_REQUEST 1

struct aos_rpc connections[MAX_CONNS];
int conn_id = 0;

void handle_connect(struct aos_rpc *r, uintptr_t pid, uintptr_t core_id, uintptr_t client_core, struct capref client_cap, struct capref *server_cap);

void count_primes_until(struct aos_rpc *rpc, uintptr_t number, uintptr_t *n_primes);


#include <aos/waitset.h>
#include <aos/default_interfaces.h>
#define PANIC_IF_FAIL(err, msg)    \
    if (err_is_fail(err)) {        \
        USER_PANIC_ERR(err, msg);  \
    }

#define SERVICE_NAME "/myservicename"
// #define TEST_BINARY  "nameservicetest"

// extern struct aos_rpc fresh_connection;
static char *myresponse = "reply!!";
static char buffer[512];
static void server_recv_handler(void *st, void *message,
                                size_t bytes,
                                void **response, size_t *response_bytes,
                                struct capref rx_cap, struct capref *tx_cap)
{
    debug_printf("server: got a request: %s\n", (char *)message);
    *response = myresponse;
    *response_bytes = strlen(myresponse);
}



int main(int argc, char *argv[])
{
    

    aos_rpc_register_handler(get_init_rpc(), AOS_RPC_BINDING_REQUEST, handle_connect);

    errval_t err;
    // debug_printf("Server\n");
    
    // char * name;
    // aos_rpc_process_get_name(aos_rpc_get_process_channel(),0,&name);
    // debug_printf("Revecived name %s\n",name);

    // coreid_t core;
    // err = aos_rpc_call(get_ns_rpc(),NS_GET_PROC_CORE,0,&core);
    // debug_printf("Revecived core %d\n",core);

    // domainid_t * pids;
    // size_t pid_count;
    // err = aos_rpc_process_get_all_pids(aos_rpc_get_process_channel(),&pids,&pid_count);
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"failed to list all pids");
    // }   

    // for(size_t i = 0; i < pid_count;++i){
    //     debug_printf("Pid %d\n",pids[i]);
    // }



    
    // debug_printf("register with nameservice '%s'\n", SERVICE_NAME);
    // debug_printf("0x%lx\n", get_init_rpc());
    
    strcpy(buffer,SERVICE_NAME);
    strcat(buffer,argv[1]);
    err = nameservice_register_properties(buffer, server_recv_handler,NULL,false,"type=ethernet,mac=1:34:124:1");
    // err = nameservice_register(buffer, server_recv_handler,NULL);
    PANIC_IF_FAIL(err, "failed to register...\n");

    // err = aos_rpc_process_spawn(get_init_rpc(),"client",!disp_get_core_id(),&my_pid);
    // if(err_is_fail(err)){
    //     DEBUG_ERR(err,"Failed to spawn client!\n");
    // // // }
    domainid_t my_pid;
    err = aos_rpc_process_spawn(get_init_rpc(),"client",1,&my_pid);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to spawn client!\n");
    }
    
    
    // nameservice_deregister(buffer);

    

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


static bool is_prime(uint64_t number)
{
    for (uint64_t divisor = 1; divisor * divisor <= number; divisor++) {
        if (divisor * divisor == number) {
            return true;
        }
    }
    return false;
}


void count_primes_until(struct aos_rpc *rpc, uintptr_t number, uintptr_t *n_primes)
{
    *n_primes = 0;
    for (uint64_t i = 1; i <= number; i++) {
        if (is_prime(i)) (*n_primes)++;
    }
}


void handle_connect(struct aos_rpc *r, uintptr_t pid, uintptr_t core_id,uintptr_t client_core ,struct capref client_cap,struct capref * server_cap){
    debug_printf("DEPRECATED!\n");
}
    /*struct capref self_ep_cap = (struct capref) {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_SELFEP
    };



   
    debug_printf("got asdfasdf!\n");
    // struct aos_rpc* rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
    

    // static struct aos_rpc new_rpc;
    // struct aos_rpc * rpc = &(connections[conn_id++]);
    //initialize_general_purpose_handler(rpc);


    if(client_core == disp_get_current_core_id()){//lmp channel
        struct lmp_endpoint * lmp_ep;
        err = endpoint_create(256,&self_ep_cap,&lmp_ep);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to create ep in memory server\n");
        }
        err = aos_rpc_init_lmp(rpc,self_ep_cap,client_cap,lmp_ep);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to register waitset on rpc\n");
        }

        // char buf[512];
        // debug_print_cap_at_capref(buf,512,self_ep_cap);
        // debug_printf("Cap her %s\n",buf);
        *server_cap = self_ep_cap;
        // debug_print_cap_at_capref(buf,512,*server_cap);
        // debug_printf("Here server cap her %s\n",buf);
    }else {
        char *urpc_data = NULL;
        err = paging_map_frame_complete(get_current_paging_state(), (void **) &urpc_data, client_cap, NULL, NULL);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to map urpc frame for ump channel into virtual address space\n");
        }
        err =  aos_rpc_init_ump_default(rpc,(lvaddr_t) urpc_data, BASE_PAGE_SIZE,false);//take second half as creating process
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to init_ump_default\n");
        } 
        *server_cap = client_cap; //NOTE: this is fucking stupid
    }


    err = aos_rpc_init(rpc);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to init rpc\n");
    }

    aos_rpc_initialize_binding(rpc, CALCULATE_REQUEST, 1, 1, AOS_RPC_WORD, AOS_RPC_WORD);
    aos_rpc_register_handler(rpc, CALCULATE_REQUEST, &count_primes_until);
    
    debug_printf("Channel established on server!\n");
}*/

