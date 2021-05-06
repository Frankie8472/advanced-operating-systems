#include <stdio.h>

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/waitset.h>


// extern struct aos_rpc fresh_connection;

int main(int argc, char *argv[])
{
    
    // printf("Hello, world! from userspace\n");
    // printf("%s\n", argv[1]);
    // stack_overflow();

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
