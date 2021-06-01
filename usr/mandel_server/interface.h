#ifndef MANDEL_SERVER_INTERFACE_H
#define MANDEL_SERVER_INTERFACE_H

#include <aos/aos_rpc.h>
#include "calculate.h"

enum {
    MS_IFACE_CALC = AOS_RPC_MSG_TYPE_START,
    MS_IFACE_N_FUNCTIONS, // <- count -- must be last
};


static struct aos_rpc_interface ms_interface;
static struct aos_rpc_function_binding ms_bindings[MS_IFACE_N_FUNCTIONS];

static struct aos_rpc_interface *get_ms_interface(void)
{
    if (ms_interface.n_bindings == 0) {
        ms_interface.n_bindings = MS_IFACE_N_FUNCTIONS;
        ms_interface.bindings = ms_bindings;

        aos_rpc_initialize_binding(&ms_interface, "roundtrip", AOS_RPC_ROUNDTRIP,
                                0, 0);

        aos_rpc_initialize_binding(&ms_interface, "calc", MS_IFACE_CALC,
                                1, 1, AOS_RPC_VARBYTES, AOS_RPC_VARBYTES);
    }
    return &ms_interface;
}


#endif // MANDEL_SERVER_INTERFACE_H
