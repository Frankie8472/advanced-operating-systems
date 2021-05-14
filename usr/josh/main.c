#include <aos/aos_rpc.h>

#include <stdio.h>


int main(int argc, char **argv)
{
    errval_t err;

    debug_printf("Welcome to JameOS Shell\n");

    printf("printing this, this can be a very long text, it is just not that efficient but doable\n");


    struct waitset *default_ws = get_default_waitset();
    /*
    for (int i = 0; i < 2; i++) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
        debug_printf("josh event %d\n", i);
    }
    thread_yield();
    thread_yield();
    thread_yield();
    thread_yield();
    thread_yield();
    for (int i = 0; i < 200000000; i++) {
        __asm volatile("mov x8, x8\n");
    }

    debug_printf("reading char...");
    char c = getchar();
    debug_printf("read chan '%c'\n", c);*/

    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
}
