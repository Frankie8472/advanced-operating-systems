#include <aos/aos_rpc.h>
#include <aos/dispatcher_rpc.h>

#include <stdio.h>


static void print_prompt(void)
{
    printf("$ \033[C\033[C\033[C");
    fflush(stdout);
}

static void on_input(void *arg)
{
    struct aos_datachan *chan = arg;
    char c = getchar();
    //debug_printf("%d\n", c);
    if (c == 13) {
        printf("\n");
        fflush(stdout);
        print_prompt();
    }
    if (c == 033) {
        //c = getchar();
    }
    else {
        printf("%c", c);
    }
    fflush(stdout);

    lmp_chan_register_recv(&chan->channel.lmp, get_default_waitset(), MKCLOSURE(on_input, arg));
}


int main(int argc, char **argv)
{
    errval_t err;
    struct waitset *default_ws = get_default_waitset();

    printf("Welcome to JameOS Shell\n");

    print_prompt();

    lmp_chan_register_recv(&stdin_chan.channel.lmp, get_default_waitset(), MKCLOSURE(on_input, &stdin_chan));

    while (true) {
        err = event_dispatch(default_ws);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch");
            abort();
        }
    }
}
