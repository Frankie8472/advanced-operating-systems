#include <stdio.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>


int main(int argc, char *argv[])
{   

    errval_t err;
    struct aos_rpc * init_rpc = get_init_rpc();

    err = aos_rpc_call(init_rpc,AOS_RPC_SERVICE_ON,1);

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
