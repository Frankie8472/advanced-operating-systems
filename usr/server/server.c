#include <stdio.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>
#include <aos/default_interfaces.h>

// extern struct aos_rpc fresh_connection;

int main(int argc, char *argv[])
{
    
    // printf("Hello, world! from userspace\n");
    // printf("%s\n", argv[1]);
    // stack_overflow();

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
