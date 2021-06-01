#include "running.h"
#include <aos/default_interfaces.h>
#include <aos/dispatcher_rpc.h>

bool do_detatch = false;

void on_process_input(void *arg) {
    struct aos_datachan *chan = arg;
    errval_t err;

    char buffer[1024];
    size_t recvd;
    do {
        err = aos_dc_receive_available(chan, sizeof buffer, buffer, &recvd);
        if (recvd > 0) {
            err = aos_dc_send(&stdout_chan, recvd, buffer);
        }
    } while(recvd > 0);

    if (aos_dc_is_closed(chan)) {
        return;
    }

    aos_dc_register(chan, get_default_waitset(), MKCLOSURE(on_process_input, arg));
}

void on_console_input(void *arg) {
    struct running_program *prog = arg;
    errval_t err;

    char buffer[1024];
    size_t recvd;
    do {
        err = aos_dc_receive_available(&stdin_chan, sizeof buffer, buffer, &recvd);
        if (recvd > 0) {
            if (!prog->is_builtin && buffer[0] == 3) { // Ctrl+C
                aos_rpc_call(&prog->process_disprpc, DISP_IFACE_TERMINATE);
            }
            if (buffer[0] == 4) { // Ctrl+D
                aos_dc_close(&prog->process_in);
            }
            if (buffer[0] == 26) { // Ctrl+Z
                do_detatch = true;
            }
            else {
                err = aos_dc_send(&prog->process_in, recvd, buffer);
            }
        }
    } while(recvd > 0);

    aos_dc_register(&stdin_chan, get_default_waitset(), MKCLOSURE(on_console_input, arg));
}


void display_running_process(struct running_program *input_receiver, struct aos_datachan *to_print)
{
    do_detatch = false;
    aos_dc_register(to_print, get_default_waitset(), MKCLOSURE(on_process_input, to_print));
    aos_dc_register(&stdin_chan, get_default_waitset(), MKCLOSURE(on_console_input, input_receiver));
    errval_t err;
    while(!aos_dc_is_closed(to_print) && !do_detatch) {
        err = event_dispatch(get_default_waitset());
    }
    aos_dc_deregister(to_print);
    aos_dc_deregister(&stdin_chan);
}
