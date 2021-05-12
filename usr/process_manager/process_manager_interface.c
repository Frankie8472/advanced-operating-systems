#include "process_manager_interface.h"

static bool initialized = false;
static struct aos_rpc_interface pm_interface;
static struct aos_rpc_function_binding bindings[PM_IFACE_N_FUNCTIONS];

static void initialize_pm_interface(void)
{
    pm_interface.n_bindings = PM_IFACE_N_FUNCTIONS;
    pm_interface.bindings = bindings;

    aos_rpc_initialize_binding(&pm_interface, "initiate", AOS_RPC_INITIATE,
                               1, 0, AOS_RPC_CAPABILITY);
    aos_rpc_initialize_binding(&pm_interface, "register_process", PM_IFACE_REGISTER_PROCESS,
                               2, 1, AOS_RPC_VARSTR, AOS_RPC_WORD, AOS_RPC_WORD);
}


struct aos_rpc_interface *get_pm_interface(void)
{
    if (!initialized) {
        initialize_pm_interface();
    }

    return &pm_interface;
}