#include <aos/default_interfaces.h>


static bool initialized = false;

static struct aos_rpc_interface init_interface;
static struct aos_rpc_function_binding bindings[INIT_IFACE_N_FUNCTIONS];

static struct aos_rpc_interface dispatcher_interface;
static struct aos_rpc_function_binding dispatcher_bindings[DISP_IFACE_N_FUNCTIONS];

static struct aos_rpc_interface write_interface;
static struct aos_rpc_function_binding write_bindings[WRITE_IFACE_N_FUNCTIONS];

static struct aos_rpc_interface memory_server_interface;
static struct aos_rpc_function_binding memory_server_bindings[MM_IFACE_N_FUNCTIONS];


static struct aos_rpc_interface name_server_interface;
static struct aos_rpc_function_binding name_server_bindings[NS_IFACE_N_FUNCTIONS];


static struct aos_rpc_interface opaque_server_interface;
static struct aos_rpc_function_binding opaque_server_bindings[OS_IFACE_N_FUNCTIONS];
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

    aos_rpc_initialize_binding(&init_interface, "NS ON", INIT_NAMESERVER_ON, 0, 0);

    aos_rpc_initialize_binding(&init_interface, "spawn", INIT_IFACE_SPAWN,
                               2, 1, AOS_RPC_VARSTR, AOS_RPC_WORD, AOS_RPC_WORD);
    aos_rpc_initialize_binding(&init_interface, "spawn_extended", INIT_IFACE_SPAWN_EXTENDED,
                               3, 1, AOS_RPC_VARBYTES, AOS_RPC_WORD, AOS_RPC_CAPABILITY, AOS_RPC_WORD);


    aos_rpc_initialize_binding(&init_interface, "reg_prc", INIT_REG_NAMESERVER, 4, 1, AOS_RPC_WORD, AOS_RPC_VARSTR,AOS_RPC_CAPABILITY,AOS_RPC_WORD,AOS_RPC_CAPABILITY);
    
    aos_rpc_initialize_binding(&init_interface, "reg_server", INIT_REG_SERVER, 4, 1,AOS_RPC_WORD,AOS_RPC_WORD, AOS_RPC_VARSTR, AOS_RPC_CAPABILITY, AOS_RPC_VARSTR);


    aos_rpc_initialize_binding(&init_interface, "lookup_server", INIT_NAME_LOOKUP, 1, 3, AOS_RPC_VARSTR,AOS_RPC_WORD,AOS_RPC_WORD, AOS_RPC_CAPABILITY);

    aos_rpc_initialize_binding(&init_interface, "forward client-server rpc", INIT_CLIENT_CALL,3,2,AOS_RPC_WORD,AOS_RPC_VARSTR,AOS_RPC_CAPABILITY,AOS_RPC_VARSTR,AOS_RPC_CAPABILITY);


    aos_rpc_initialize_binding(&init_interface, "multi-hop-init", INIT_MULTI_HOP_CON,2,1,AOS_RPC_VARSTR,AOS_RPC_CAPABILITY,AOS_RPC_CAPABILITY);





    // ===================== Dispatcher Interface =====================

    dispatcher_interface.n_bindings = DISP_IFACE_N_FUNCTIONS;
    dispatcher_interface.bindings = dispatcher_bindings;

    aos_rpc_initialize_binding(&dispatcher_interface, "binding", DISP_IFACE_BINDING,
                               1, 0, AOS_RPC_CAPABILITY);
    aos_rpc_initialize_binding(&dispatcher_interface, "rebind", DISP_IFACE_REBIND,
                               1, 0, AOS_RPC_CAPABILITY);
    aos_rpc_initialize_binding(&dispatcher_interface, "set_stdout", DISP_IFACE_SET_STDOUT,
                               1, 0, AOS_RPC_CAPABILITY);
    aos_rpc_initialize_binding(&dispatcher_interface, "get_stdin", DISP_IFACE_GET_STDIN,
                               0, 1, AOS_RPC_CAPABILITY);

    // ===================== Write Interface =====================

    write_interface.n_bindings = WRITE_IFACE_N_FUNCTIONS;
    write_interface.bindings = write_bindings;

    aos_rpc_initialize_binding(&write_interface, "write_varbytes", WRITE_IFACE_WRITE_VARBYTES,
                               1, 0, AOS_RPC_VARBYTES);






    // ===================== Memory Server Interface =====================

    memory_server_interface.n_bindings = DISP_IFACE_N_FUNCTIONS;
    memory_server_interface.bindings = memory_server_bindings;

    aos_rpc_initialize_binding(&memory_server_interface, "initiate", AOS_RPC_INITIATE,
                               1, 0, AOS_RPC_CAPABILITY);
    aos_rpc_initialize_binding(&memory_server_interface, "get_ram", MM_IFACE_GET_RAM,
                               2, 2, AOS_RPC_WORD, AOS_RPC_WORD, AOS_RPC_CAPABILITY, AOS_RPC_WORD);






    // ===================== Name Server Interface ========================

    name_server_interface.n_bindings = NS_IFACE_N_FUNCTIONS;
    name_server_interface.bindings = name_server_bindings;


    aos_rpc_initialize_binding(&name_server_interface,"get_proc_name",NS_GET_PROC_NAME,1,1,AOS_RPC_WORD,AOS_RPC_VARSTR);
    aos_rpc_initialize_binding(&name_server_interface,"get_proc_core",NS_GET_PROC_CORE,1,1,AOS_RPC_WORD,AOS_RPC_WORD);
    aos_rpc_initialize_binding(&name_server_interface,"get_proc_list",NS_GET_PROC_LIST,0,2,AOS_RPC_WORD,AOS_RPC_VARSTR);
    aos_rpc_initialize_binding(&name_server_interface,"get_pid",NS_GET_PID,0,1,AOS_RPC_WORD);
    aos_rpc_initialize_binding(&name_server_interface,"dereg server",NS_DEREG_SERVER,1,1,AOS_RPC_VARSTR,AOS_RPC_WORD);




    // ===================== Opaque Server Interface ========================
    opaque_server_interface.n_bindings = OS_IFACE_N_FUNCTIONS;
    opaque_server_interface.bindings = opaque_server_bindings;
    aos_rpc_initialize_binding(&opaque_server_interface,"server_message",OS_IFACE_MESSAGE,2,2,AOS_RPC_VARSTR,AOS_RPC_CAPABILITY,AOS_RPC_VARSTR,AOS_RPC_CAPABILITY);

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
    return &dispatcher_interface;
}


struct aos_rpc_interface *get_write_interface(void)
{
    if (!initialized) {
        initialize_interfaces();
    }

    return &write_interface;
}


struct aos_rpc_interface *get_memory_server_interface(void)
{
    if (!initialized) {
        initialize_interfaces();
    }

    return &memory_server_interface;
}

struct aos_rpc_interface *get_nameserver_interface(void)
{
    if (!initialized) {
        initialize_interfaces();
    }

    return &name_server_interface;
}

struct aos_rpc_interface * get_opaque_server_interface(void){
    if (!initialized) {
        initialize_interfaces();
    }

    return &opaque_server_interface;
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
