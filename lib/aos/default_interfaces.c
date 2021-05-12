#include <aos/default_interfaces.h>


static bool initialized = false;
static struct aos_rpc_interface init_interface;
static struct aos_rpc_function_binding bindings[INIT_IFACE_N_FUNCTIONS];

static void initialize_init_interface(void)
{
    init_interface.n_bindings = INIT_IFACE_N_FUNCTIONS;
    init_interface.bindings = bindings;

    aos_rpc_initialize_binding(&init_interface, "initiate", INIT_IFACE_INITIATE,
                               1, 0, AOS_RPC_CAPABILITY);
    aos_rpc_initialize_binding(&init_interface, "get_ram", INIT_IFACE_GET_RAM,
                               2, 2, AOS_RPC_WORD, AOS_RPC_WORD, AOS_RPC_CAPABILITY, AOS_RPC_WORD);
    aos_rpc_initialize_binding(&init_interface, "spawn", INIT_IFACE_SPAWN,
                               2, 1, AOS_RPC_VARSTR, AOS_RPC_WORD, AOS_RPC_WORD);
}


struct aos_rpc_interface *get_init_interface(void)
{
    if (!initialized) {
        initialize_init_interface();
    }

    return &init_interface;
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
