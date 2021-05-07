#include <stdio.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>

#define MAX_CONNS 16

#define CALCULATE_REQUEST 1

struct aos_rpc connections[MAX_CONNS];
int conn_id = 0;

void handle_connect(struct aos_rpc *r, uintptr_t pid, uintptr_t core_id, uintptr_t client_core, struct capref client_cap, struct capref *server_cap);

void count_primes_until(struct aos_rpc *rpc, uintptr_t number, uintptr_t *n_primes);


// extern struct aos_rpc fresh_connection;

int main(int argc, char *argv[])
{
    // printf("Hello, world! from userspace\n");
    // printf("%s\n", argv[1]);
    // stack_overflow();

    aos_rpc_register_handler(get_init_rpc(), AOS_RPC_BINDING_REQUEST, handle_connect);

    errval_t err;
    debug_printf("Server\n");



    // malloc(sizeof(struct aos_rpc));
    


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
    errval_t err;
    struct capref self_ep_cap = (struct capref) {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_SELFEP
    };



   
    debug_printf("got asdfasdf!\n");
    // struct aos_rpc* rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
    

    // static struct aos_rpc new_rpc;
    struct aos_rpc * rpc = &(connections[conn_id++]);
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
}

