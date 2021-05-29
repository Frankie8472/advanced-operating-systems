#include <stdio.h>
#include <stdlib.h>


#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/default_interfaces.h>
#include <aos/deferred.h>
#include <aos/waitset.h>


#include "process_list.h"
#include "nameserver_handlers.h"
#include "server_list.h"
#include <hashtable/hashtable.h>


static void sweep_server_list(void * ptr){
    struct server_list* curr = servers;
    while(curr != NULL){
        if(curr -> marked == true){
            debug_printf("Deleting server (Dead service removal) : %s\n", curr -> name);
            struct server_list * temp = curr;
            curr = curr -> next;
            remove_server(temp);
            print_server_list();
        }else{
            curr -> marked = true;
            curr = curr -> next;
        }
    }
}

int main(int argc, char *argv[])
{   


    process = 2;
    n_servers = 0;
    errval_t err;

    pl.head = NULL;
    pl.tail = NULL;
    pl.size = 0;

    server_ht = create_hashtable();
    // domainid_t my_pid;
    
    err = add_process(0,"nameserver",0,NULL);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to add ns to process list!\n");
    }

    struct aos_rpc * init_rpc = get_init_rpc();
    initialize_ns_handlers(init_rpc);
    err = aos_rpc_call(init_rpc,INIT_NAMESERVER_ON);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to call namerserver online!\n");
    }

    struct periodic_event pe;
    err = periodic_event_create(&pe,get_default_waitset(),NS_SWEEP_INTERVAL,MKCLOSURE(sweep_server_list,NULL));
    struct waitset *default_ws = get_default_waitset();
    debug_printf("Message handler loop\n");
    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            debug_printf("err in event_dispatch\n");
            abort();
        }
    }
    return 0;
}
