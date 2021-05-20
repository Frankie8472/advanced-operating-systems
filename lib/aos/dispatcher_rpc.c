#include <aos/dispatcher_rpc.h>
#include <aos/default_interfaces.h>
#include <aos/caddr.h>
#include <aos/deferred.h>

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
    //debug_printf("handle_set_stdout\n");
    slot_free(stdout_chan.channel.lmp.remote_cap);
    stdout_chan.channel.lmp.remote_cap = new_stdout_ep;
}


static void handle_get_stdin(struct aos_rpc *rpc, struct capref *stdin_ep)
{
    //debug_printf("handle_get_stdin\n");
    *stdin_ep = stdin_chan.channel.lmp.local_cap;
}

static void handle_terminate(struct aos_rpc *rpc)
{
    void finito(void *a) { exit(1); }

    struct deferred_event *de = malloc(sizeof(struct deferred_event));
    deferred_event_init(de);
    deferred_event_register(de, get_default_waitset(), 0, MKCLOSURE(finito, NULL));
}


static void initialize_dispatcher_handlers(struct aos_rpc *dr)
{
    aos_rpc_register_handler(dr, DISP_IFACE_REBIND, handle_rebind);
    aos_rpc_register_handler(dr, DISP_IFACE_SET_STDOUT, handle_set_stdout);
    aos_rpc_register_handler(dr, DISP_IFACE_GET_STDIN, handle_get_stdin);
    aos_rpc_register_handler(dr, DISP_IFACE_TERMINATE, handle_terminate);
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
    // debug_printf("Trying to establish channel with init (or memory server with client):\n");
    err = aos_rpc_init_lmp(&init_rpc, NULL_CAP, init_ep_cap, NULL, NULL);
    // debug_printf("Trying to establish channel with init (or memory server with client):\n");
    // err = aos_rpc_init_lmp(&init_rpc, self_ep_cap, init_ep_cap, NULL, NULL);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "Error establishing connection with init! aborting!");
        abort();
    }
    err = aos_rpc_call(&init_rpc, AOS_RPC_INITIATE, init_rpc.channel.lmp.local_cap);
    if (!err_is_fail(err)) {
        // debug_printf("init channel established!\n");
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



    // setup stdin
    struct capref stdin_epcap;
    struct lmp_endpoint *stdin_endpoint;
    err = endpoint_create(LMP_RECV_LENGTH * 8, &stdin_epcap, &stdin_endpoint);
    err = aos_dc_init_lmp(&stdin_chan, 1024);
    stdin_chan.channel.lmp.endpoint = stdin_endpoint;
    stdin_chan.channel.lmp.local_cap = stdin_epcap;



    // setting up dispatcher rpc
    err = aos_rpc_init_lmp(&dispatcher_rpc, NULL_CAP, spawner_ep_cap, NULL, NULL);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "Error establishing connection with memory server! aborting!");
        abort();
    }

    //initialize_dispatcher_handlers(&dispatcher_rpc);


    initialize_dispatcher_handlers(&dispatcher_rpc);

    struct capref real_stdout_ep_cap = stdout_ep_cap;

    struct capability disp_rpc_ep;
    invoke_cap_identify(dispatcher_rpc.channel.lmp.remote_cap, &disp_rpc_ep);
    if (disp_rpc_ep.type == ObjType_EndPointLMP) {
        debug_printf("binding spawner\n");
        struct capref new_stdout;
        err = aos_rpc_call(&dispatcher_rpc, DISP_IFACE_BINDING, dispatcher_rpc.channel.lmp.local_cap, stdin_epcap, &new_stdout);
        if (!capref_is_null(new_stdout)) {
            real_stdout_ep_cap = new_stdout;
        }
        if (err_is_fail(err)) {
            debug_printf("error binding to spawner endpoint\n");
        }
    }
    else {

    }

    aos_dc_init_lmp(&stdout_chan, 64);

    struct capability stdout_cap;
    invoke_cap_identify(real_stdout_ep_cap, &stdout_cap);
    if (stdout_cap.type == ObjType_EndPointLMP) {
        stdout_chan.channel.lmp.remote_cap = real_stdout_ep_cap;
    }
    else {
        stdout_chan.channel.lmp.remote_cap = NULL_CAP;
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



errval_t init_nameserver_rpc(char * name){
    // debug_printf("Calling init ns with name ========================================================== %s\n",name);
    errval_t err;
    struct aos_rpc* ns_rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
    struct capref ns_cap;
    err = slot_alloc(&ns_cap);
    ON_ERR_RETURN(err);
    domainid_t my_pid = disp_get_domain_id();
    // domainid_t my_pid;
    if(disp_get_core_id() == 0){ //lmp
        struct lmp_endpoint *name_server_ep;
        err = endpoint_create(LMP_RECV_LENGTH, &ns_cap, &name_server_ep); //TODO: maybe a longer recv length is great here for getting list of all servers? however we hve alot of these so maybe not
        ON_ERR_RETURN(err);
        err = aos_rpc_init_lmp(ns_rpc,ns_cap,NULL_CAP,name_server_ep,get_default_waitset());
        ON_ERR_RETURN(err);

        struct capref remote_ns_cap;
        err = aos_rpc_call(get_init_rpc(),INIT_REG_NAMESERVER,disp_get_core_id(),name,ns_cap,my_pid,&remote_ns_cap);
        ON_ERR_RETURN(err);
        ns_rpc -> channel.lmp.remote_cap = remote_ns_cap;
    }
    else { //ump
        size_t urpc_cap_size;
        err  = frame_alloc(&ns_cap,BASE_PAGE_SIZE,&urpc_cap_size);
        ON_ERR_RETURN(err);
        char *urpc_data = NULL;
        err = paging_map_frame_complete(get_current_paging_state(), (void **) &urpc_data, ns_cap, NULL, NULL);
        ON_ERR_RETURN(err);
        err =  aos_rpc_init_ump_default(ns_rpc,(lvaddr_t) urpc_data, BASE_PAGE_SIZE,true);//take first half as ns takes second half
        ON_ERR_RETURN(err);
        struct capref dummy_cap; // not useed
        err = aos_rpc_call(get_init_rpc(),INIT_REG_NAMESERVER,disp_get_core_id(),name,ns_cap,my_pid,&dummy_cap);
        ON_ERR_RETURN(err);
    }

    err = aos_rpc_set_interface(ns_rpc,get_nameserver_interface(),0,NULL);
    ON_ERR_RETURN(err);
    set_ns_rpc(ns_rpc);

    return SYS_ERR_OK;
}