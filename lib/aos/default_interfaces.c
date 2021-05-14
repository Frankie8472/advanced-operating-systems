#include <aos/default_interfaces.h>


static bool initialized = false;

static struct aos_rpc_interface init_interface;
static struct aos_rpc_function_binding bindings[INIT_IFACE_N_FUNCTIONS];


static struct aos_rpc_interface dispatcher_interface;
static struct aos_rpc_function_binding dispatcher_bindings[DISP_IFACE_N_FUNCTIONS];

static struct aos_rpc_interface memory_server_interface;
static struct aos_rpc_function_binding memory_server_bindings[MM_IFACE_N_FUNCTIONS];

static void initialize_interfaces(void)
{



    // ===================== Init Interface =====================
    init_interface.n_bindings = INIT_IFACE_N_FUNCTIONS;
    init_interface.bindings = bindings;

    aos_rpc_initialize_binding(&init_interface, "initiate", AOS_RPC_INITIATE,
                               1, 0, AOS_RPC_CAPABILITY);
    aos_rpc_initialize_binding(&init_interface, "send_number", AOS_RPC_SEND_NUMBER, 1, 0, AOS_RPC_WORD);
    aos_rpc_initialize_binding(&init_interface, "send_string" ,AOS_RPC_SEND_STRING, 1, 0, AOS_RPC_VARSTR);
    aos_rpc_initialize_binding(&init_interface, "varbytes" ,AOS_RPC_SEND_VARBYTES, 1, 0, AOS_RPC_VARBYTES);
    aos_rpc_initialize_binding(&init_interface, "putchar" ,AOS_RPC_PUTCHAR, 1, 0, AOS_RPC_WORD);
    aos_rpc_initialize_binding(&init_interface, "getchar", AOS_RPC_GETCHAR, 0, 1, AOS_RPC_WORD);
    aos_rpc_initialize_binding(&init_interface, "binding_reqeust",AOS_RPC_BINDING_REQUEST,4,1,AOS_RPC_WORD,AOS_RPC_WORD,AOS_RPC_WORD,AOS_RPC_CAPABILITY,AOS_RPC_CAPABILITY);
    aos_rpc_initialize_binding(&init_interface, "round_trip",AOS_RPC_ROUNDTRIP, 0, 0);

    aos_rpc_initialize_binding(&init_interface, "spawn", INIT_IFACE_SPAWN,
                               2, 1, AOS_RPC_VARSTR, AOS_RPC_WORD, AOS_RPC_WORD);




    // ===================== Dispatcher Interface =====================

    dispatcher_interface.n_bindings = DISP_IFACE_N_FUNCTIONS;
    dispatcher_interface.bindings = dispatcher_bindings;

    // parameters: epcap for dispatcher interface, epcap for stdin, epcap for stdout
    aos_rpc_initialize_binding(&dispatcher_interface, "binding", DISP_IFACE_BINDING,
                               2, 1, AOS_RPC_CAPABILITY, AOS_RPC_CAPABILITY, AOS_RPC_CAPABILITY);
    aos_rpc_initialize_binding(&dispatcher_interface, "rebind", DISP_IFACE_REBIND,
                               1, 0, AOS_RPC_CAPABILITY);
    aos_rpc_initialize_binding(&dispatcher_interface, "set_stdout", DISP_IFACE_SET_STDOUT,
                               1, 0, AOS_RPC_CAPABILITY);
    aos_rpc_initialize_binding(&dispatcher_interface, "get_stdin", DISP_IFACE_GET_STDIN,
                               0, 1, AOS_RPC_CAPABILITY);




    // -> stub compiler 


    // ===================== Memory Server Interface =====================

    memory_server_interface.n_bindings = DISP_IFACE_N_FUNCTIONS;
    memory_server_interface.bindings = memory_server_bindings;

    aos_rpc_initialize_binding(&memory_server_interface, "initiate", AOS_RPC_INITIATE,
                               1, 0, AOS_RPC_CAPABILITY);
    aos_rpc_initialize_binding(&memory_server_interface, "get_ram", MM_IFACE_GET_RAM,
                               2, 2, AOS_RPC_WORD, AOS_RPC_WORD, AOS_RPC_CAPABILITY, AOS_RPC_WORD);

    initialized = true;
}


struct aos_rpc_interface *get_init_interface(void)
{
    if (!initialized) {
        initialize_interfaces();
    }

    return &init_interface;
}


struct aos_rpc_interface *get_dispatcher_interface(void)
{
    if (!initialized) {
        initialize_interfaces();
    }
    return &init_interface;
}


struct aos_rpc_interface *get_memory_server_interface(void)
{
    if (!initialized) {
        initialize_interfaces();
    }

    return &memory_server_interface;
}


void initialize_initiate_handler(struct aos_rpc *rpc)
{
    assert(rpc->backend == AOS_RPC_LMP);

    void handle_initiate(struct aos_rpc *r, struct capref remote_ep_cap) {
        debug_printf("handle_initiate\n");
        r->channel.lmp.remote_cap = remote_ep_cap;
    }

    aos_rpc_register_handler(rpc, AOS_RPC_INITIATE, handle_initiate);
}
