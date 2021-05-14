#include <aos/dispatcher_rpc.h>
#include <aos/default_interfaces.h>
#include <aos/caddr.h>

struct aos_rpc stdin_rpc;
struct aos_rpc stdout_rpc;

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
    slot_free(stdout_rpc.channel.lmp.remote_cap);
    stdout_rpc.channel.lmp.remote_cap = new_stdout_ep;
}


static void handle_get_stdin(struct aos_rpc *rpc, struct capref *stdin_ep)
{
    *stdin_ep = stdin_rpc.channel.lmp.local_cap;
}


static void initialize_dispatcher_handlers(struct aos_rpc *disp_rpc)
{
    aos_rpc_register_handler(disp_rpc, DISP_IFACE_REBIND, handle_rebind);
    aos_rpc_register_handler(disp_rpc, DISP_IFACE_SET_STDOUT, handle_set_stdout);
    aos_rpc_register_handler(disp_rpc, DISP_IFACE_GET_STDIN, handle_get_stdin);
}


errval_t init_dispatcher_rpcs(void)
{
    errval_t err;

    struct capref self_ep_cap = {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_SELFEP
    };

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

    struct capref stdout_ep_cap = {
        .cnode = cnode_task,
        .slot = TASKCN_SLOT_STDOUT_EP
    };

    err = aos_rpc_set_interface(&init_rpc, get_init_interface(), INIT_IFACE_N_FUNCTIONS, init_rpc_handlers);
    ON_ERR_RETURN(err);
    err = aos_rpc_set_interface(&mm_rpc, get_memory_server_interface(), 0, NULL);
    ON_ERR_RETURN(err);

    // Establishing channel with init
    debug_printf("Trying to establish channel with init (or memory server with client):\n");
    err = aos_rpc_init_lmp(&init_rpc, self_ep_cap, init_ep_cap, NULL, NULL);
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
    struct capref epcap;
    struct lmp_endpoint *stdout_endpoint;

    endpoint_create(LMP_RECV_LENGTH, &epcap, &stdout_endpoint);
    err = aos_rpc_init_lmp(&stdout_rpc, epcap, stdout_ep_cap, stdout_endpoint, NULL);


    struct capref stdin_epcap;
    struct lmp_endpoint *stdin_endpoint;

    endpoint_create(LMP_RECV_LENGTH * 8, &stdin_epcap, &stdin_endpoint);
    err = aos_rpc_init_lmp(&stdout_rpc, stdin_epcap, NULL_CAP, stdin_endpoint, NULL);
    err = aos_rpc_set_interface(&stdout_rpc, get_write_interface(), 0, NULL);



    // setting up dispatcher rpc
    err = aos_rpc_init_lmp(&dispatcher_rpc, NULL_CAP, spawner_ep_cap, NULL, NULL);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "Error establishing connection with memory server! aborting!");
        abort();
    }

    err = aos_rpc_set_interface(&dispatcher_rpc, get_dispatcher_interface(), DISP_IFACE_N_FUNCTIONS, dispatcher_rpc_handlers);
    ON_ERR_RETURN(err);

    initialize_dispatcher_handlers(&dispatcher_rpc);


    //err = aos_rpc_call(&dispatcher_rpc, DISP_IFACE_BINDING, dispatcher_rpc.channel.lmp.local_cap,
    //                   stdin_epcap, &stdout_rpc.channel.lmp.remote_cap);
    struct capability rem_cap;
    invoke_cap_identify(dispatcher_rpc.channel.lmp.remote_cap, &rem_cap);
    if (rem_cap.type == ObjType_EndPointLMP) {
        debug_printf("initiating!\n");
        err = aos_rpc_call(&dispatcher_rpc, AOS_RPC_INITIATE, dispatcher_rpc.channel.lmp.local_cap);
    }

    return SYS_ERR_OK;
}