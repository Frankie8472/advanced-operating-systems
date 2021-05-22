#include "running.h"
#include <aos/default_interfaces.h>
#include <aos/dispatcher_rpc.h>


void on_process_input(void *arg) {
    struct running_program *prog = arg;
    errval_t err;

    char buffer[1024];
    size_t recvd;
    do {
        err = aos_dc_receive_available(&prog->process_out, sizeof buffer, buffer, &recvd);
        if (recvd > 0) {
            err = aos_dc_send(&stdout_chan, recvd, buffer);
        }
    } while(recvd > 0);

    if (aos_dc_is_closed(&prog->process_out)) {
        return;
    }

    aos_dc_register(&prog->process_out, get_default_waitset(), MKCLOSURE(on_process_input, arg));
}

void on_console_input(void *arg) {
    struct running_program *prog = arg;
    errval_t err;

    char buffer[1024];
    size_t recvd;
    do {
        err = aos_dc_receive_available(&stdin_chan, sizeof buffer, buffer, &recvd);
        if (recvd > 0) {
            if (buffer[0] == 3) {
                aos_rpc_call(&prog->process_disprpc, DISP_IFACE_TERMINATE);
            }
            else {
                err = aos_dc_send(&prog->process_in, recvd, buffer);
            }
        }
    } while(recvd > 0);

    aos_dc_register(&stdin_chan, get_default_waitset(), MKCLOSURE(on_console_input, arg));
}


void process_running(struct running_program *prog)
{
    aos_dc_register(&prog->process_out, get_default_waitset(), MKCLOSURE(on_process_input, prog));
    //aos_dc_register(&stdin_chan, get_default_waitset(), MKCLOSURE(on_console_input, prog));
    lmp_endpoint_register(stdin_chan.channel.lmp.endpoint, get_default_waitset(), MKCLOSURE(on_console_input, prog));
    errval_t err;
    while(!aos_dc_is_closed(&prog->process_out)) {
        err = event_dispatch(get_default_waitset());
    }
    aos_dc_deregister(&prog->process_out);
    aos_dc_deregister(&stdin_chan);
}