#include <aos/dispatcher_rpc.h>
#include <aos/default_interfaces.h>
#include <aos/caddr.h>

struct aos_datachan stdin_chan;
struct aos_datachan stdout_chan;

/// default service every dispatcher provides
struct aos_rpc dispatcher_rpc;

static struct aos_rpc init_rpc;
static void *init_rpc_handlers[INIT_IFACE_N_FUNCTIONS];

static struct aos_rpc mm_rpc;

static void *dispatcher_rpc_handlers[DISP_IFACE_N_FUNCTIONS];


static void handle_rebind(struct aos_rpc *rpc, struct capref new_ep)
{
    slot_free(rpc->channel.lmp.remote_cap);
    rpc->channel.lmp.remote_cap = new_ep;
}


static void handle_set_stdout(struct aos_rpc *rpc, struct capref new_stdout_ep)
{
    debug_printf("handle_set_stdout\n");
    slot_free(stdout_chan.channel.lmp.remote_cap);
    stdout_chan.channel.lmp.remote_cap = new_stdout_ep;
}


static void handle_get_stdin(struct aos_rpc *rpc, struct capref *stdin_ep)
{
    debug_printf("handle_get_stdin\n");
    *stdin_ep = stdin_chan.channel.lmp.local_cap;
}


static void initialize_dispatcher_handlers(struct aos_rpc *dr)
{
    aos_rpc_register_handler(dr, DISP_IFACE_REBIND, handle_rebind);
    aos_rpc_register_handler(dr, DISP_IFACE_SET_STDOUT, handle_set_stdout);
    aos_rpc_register_handler(dr, DISP_IFACE_GET_STDIN, handle_get_stdin);
}


errval_t init_dispatcher_rpcs(void)
{
    errval_t err;

    struct capref init_ep_cap = {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_INITEP
    };

    struct capref mm_ep_cap = {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_MEMORYEP
    };

    struct capref spawner_ep_cap = {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_SPAWNER_EP
    };
    __unused
    struct capref stdout_ep_cap = {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_STDOUT_EP
    };

    err = aos_rpc_set_interface(&init_rpc, get_init_interface(), INIT_IFACE_N_FUNCTIONS, init_rpc_handlers);
    ON_ERR_RETURN(err);
    err = aos_rpc_set_interface(&mm_rpc, get_memory_server_interface(), 0, NULL);
    ON_ERR_RETURN(err);
    err = aos_rpc_set_interface(&dispatcher_rpc, get_dispatcher_interface(), DISP_IFACE_N_FUNCTIONS, dispatcher_rpc_handlers);
    ON_ERR_RETURN(err);

    // Establishing channel with init
    debug_printf("Trying to establish channel with init (or memory server with client):\n");
    err = aos_rpc_init_lmp(&init_rpc, NULL_CAP, init_ep_cap, NULL, NULL);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "Error establishing connection with init! aborting!");
        abort();
    }
    err = aos_rpc_call(&init_rpc, AOS_RPC_INITIATE, init_rpc.channel.lmp.local_cap);
    if (!err_is_fail(err)) {
        debug_printf("init channel established!\n");
    }
    else {
        DEBUG_ERR(err, "Error establishing connection with init! aborting!");
        abort();
    }
    set_init_rpc(&init_rpc);


    // Establishing channel with mm
    err = aos_rpc_init_lmp(&mm_rpc, NULL_CAP, mm_ep_cap, NULL, NULL);
    mm_rpc.lmp_server_mode = true;
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "Error establishing connection with memory server! aborting!");
        abort();
    }
    set_mm_rpc(&mm_rpc);


    // Setting up stdout endpoint
    /*struct capref epcap;
    struct lmp_endpoint *stdout_endpoint;

    endpoint_create(LMP_RECV_LENGTH, &epcap, &stdout_endpoint);
    err = aos_rpc_init_lmp(&stdout_rpc, epcap, stdout_ep_cap, stdout_endpoint, NULL);
    err = aos_rpc_set_interface(&stdout_rpc, get_write_interface(), 0, NULL);*/
    aos_dc_init(&stdout_chan, 64);

    struct capability stdout_cap;
    invoke_cap_identify(stdout_ep_cap, &stdout_cap);
    if (stdout_cap.type == ObjType_EndPointLMP) {
        stdout_chan.channel.lmp.remote_cap = stdout_ep_cap;
    }
    else {
        stdout_chan.channel.lmp.remote_cap = NULL_CAP;
    }


    // setup stdin
    struct capref stdin_epcap;
    struct lmp_endpoint *stdin_endpoint;
    err = endpoint_create(LMP_RECV_LENGTH * 8, &stdin_epcap, &stdin_endpoint);
    err = aos_dc_init(&stdin_chan, 1024);
    stdin_chan.channel.lmp.endpoint = stdin_endpoint;
    stdin_chan.channel.lmp.local_cap = stdin_epcap;



    // setting up dispatcher rpc
    err = aos_rpc_init_lmp(&dispatcher_rpc, NULL_CAP, spawner_ep_cap, NULL, NULL);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "Error establishing connection with memory server! aborting!");
        abort();
    }

    //initialize_dispatcher_handlers(&dispatcher_rpc);


    aos_rpc_register_handler(&dispatcher_rpc, DISP_IFACE_GET_STDIN, handle_get_stdin);

    initialize_dispatcher_handlers(&dispatcher_rpc);

    debug_printf("dispatcher iface: %p\n", dispatcher_rpc);
    debug_printf("n_rets: %p\n", dispatcher_rpc.interface->bindings[DISP_IFACE_GET_STDIN].n_rets);

    struct capability disp_rpc_ep;
    invoke_cap_identify(dispatcher_rpc.channel.lmp.remote_cap, &disp_rpc_ep);
    if (disp_rpc_ep.type == ObjType_EndPointLMP) {
        debug_printf("binding spawner\n");
        err = aos_rpc_call(&dispatcher_rpc, DISP_IFACE_BINDING, dispatcher_rpc.channel.lmp.local_cap);

    }

    /*struct capability rem_cap;
    invoke_cap_identify(dispatcher_rpc.channel.lmp.remote_cap, &rem_cap);
    if (rem_cap.type == ObjType_EndPointLMP) {
        debug_printf("initiating!\n");
        err = aos_rpc_call(&dispatcher_rpc, AOS_RPC_INITIATE, dispatcher_rpc.channel.lmp.local_cap);
    }
    else {
        debug_printf("not calling!\n");
    }*/

    return SYS_ERR_OK;
}