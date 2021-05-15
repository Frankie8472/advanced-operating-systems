#include <stdio.h>
#include <stdlib.h>


#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/default_interfaces.h>
#include "process_list.h"
#include "nameserver_handlers.h"



int main(int argc, char *argv[])
{   


    process = 0;

    errval_t err;
    debug_printf("Hello from nameserver!\n");

    pl.head = NULL;
    pl.tail = NULL;
    pl.size = 0;

    // domainid_t my_pid;
    
    err = add_process(0,"nameserver",process++,NULL);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to add ns to process list!\n");
    }

    struct aos_rpc * init_rpc = get_init_rpc();
    initialize_ns_handlers(init_rpc);
    err = aos_rpc_call(init_rpc,INIT_NAMESERVER_ON);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to call namerserver online!\n");
    }

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
