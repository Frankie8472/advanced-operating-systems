
#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/default_interfaces.h>
#include <aos/waitset.h>
#include <aos/coreboot.h>
#include <spawn/multiboot.h>

#include <grading.h>

#include <spawn/spawn.h>

#include <spawn/process_manager.h>
#include <mm/mm.h>
#include "rpc_server.h"
#include "spawn_server.h"
#include "mem_alloc.h"
#include "../../lib/aos/include/init.h"
#include "routing.h"




#include <aos/dispatch.h>
#include <aos/dispatcher_arch.h>
#include <aos/curdispatcher_arch.h>
#include <barrelfish_kpi/dispatcher_shared.h>
#include <barrelfish_kpi/startup_arm.h>

#include <aos/cache.h>
#include <aos/kernel_cap_invocations.h>


// #include <aos/curdispatcher_arch.h>

struct terminal_queue {
    struct lmp_chan* cur;
    struct terminal_queue* next;
};

struct terminal_state{
  bool reading;
  char to_put[1024];
  size_t index;
  struct terminal_queue* waiting;
};

static struct terminal_state *terminal_state;
static void *get_opaque_server_rpc_handlers[OS_IFACE_N_FUNCTIONS];


errval_t init_terminal_state(void)
{
    struct terminal_state ts;
    ts.reading = false;
    ts.index = 0;
    ts.waiting = NULL;
    terminal_state = &ts;
    
    return SYS_ERR_OK;
}

static struct waitset mm_waitset;

static int memory_server_func(void *arg)
{
    errval_t err;
    // debug_printf("memory server thread started\n");
    while (true) {
        //debug_printf("waiting in thread waitset\n");
        err = event_dispatch(&mm_waitset);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "in event_dispatch of memory thread");
            abort();
        }
    }
    return SYS_ERR_OK;
}

errval_t start_memory_server_thread(void)
{
    static struct aos_rpc memory_server;
    static void *handlers[MM_IFACE_N_FUNCTIONS];

    aos_rpc_set_interface(&memory_server, get_memory_server_interface(), MM_IFACE_N_FUNCTIONS, handlers);

    struct lmp_endpoint *mm_ep;
    lmp_endpoint_create_in_slot(LMP_RECV_LENGTH * 2, cap_mmep, &mm_ep);

    waitset_init(&mm_waitset);

    aos_rpc_init_lmp(&memory_server, cap_mmep, NULL_CAP, mm_ep, &mm_waitset);
    aos_rpc_register_handler(&memory_server, MM_IFACE_GET_RAM, handle_request_ram);

    memory_server.lmp_server_mode = true;

    thread_create(memory_server_func, NULL);

    return SYS_ERR_OK;
}


/**
 * \brief Establishing a RPC/UMP channel with another core
 *
 * \param coreid Coreid of foreign core
 * \param urpc_frame Shared communication frame between the two cores
 */
errval_t init_core_channel(coreid_t coreid, lvaddr_t urpc_frame)
{
    struct aos_rpc *rpc = malloc(sizeof(struct aos_rpc));
    NULLPTR_CHECK(rpc, LIB_ERR_MALLOC_FAIL);

    errval_t err;

    err = SYS_ERR_OK;
    aos_rpc_set_interface(rpc,get_init_interface(),INIT_IFACE_N_FUNCTIONS,malloc(INIT_IFACE_N_FUNCTIONS * sizeof(void *)));
    initialize_rpc_handlers(rpc);
    //err = aos_rpc_init(rpc); TODO (RPC): set intercore interface
    ON_ERR_PUSH_RETURN(err, LIB_ERR_RPC_INIT);
    aos_rpc_init_ump_default(rpc, urpc_frame, BASE_PAGE_SIZE, coreid < disp_get_core_id());
    ON_ERR_PUSH_RETURN(err, LIB_ERR_RPC_INIT);

    // register_core_channel_handlers(rpc);


    set_core_channel(coreid,rpc);
    
    return SYS_ERR_OK;
}


/**
 * \brief handler function for send number rpc call
 */
void handle_send_number(struct aos_rpc *r, uintptr_t number) {
    //debug_printf("recieved number: %ld\n", number);
    grading_rpc_handle_number(number);
}

/**
 * \brief handler function for send string rpc call
 */
void handle_send_string(struct aos_rpc *r, const char *string) {
    grading_rpc_handler_string(string);
}

/**
 * \brief handler function for putchar rpc call
 */
void handle_putchar(struct aos_rpc *r, uintptr_t c) {
    grading_rpc_handler_serial_putchar((char) c);
    putchar(c);
    //debug_printf("recieved: %c\n", (char)c);
}

/**
 * \brief handler function for getchar rpc call
 */
void handle_getchar(struct aos_rpc *r, uintptr_t *c) {
    grading_rpc_handler_serial_getchar();
    int v = getchar();
    //debug_printf("getchar: %c\n", v);
    *c = v;//getchar();
}

/**
 * \brief handler function for ram alloc rpc call
 */
void handle_request_ram(struct aos_rpc *r, uintptr_t size, uintptr_t alignment, struct capref *cap, uintptr_t *ret_size) {
    // debug_printf("handle_request_ram\n");
    grading_rpc_handler_ram_cap(size, alignment);
    errval_t err = ram_alloc_aligned(cap, size, alignment);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "Error in remote ram allocation!\n");
    }
}

/**
 * \brief handler function for initiate rpc call
 * 
 * This function is called by a domain who wishes to make use of the
 *rpc interface of the init domain
 */
void handle_initiate(struct aos_rpc *rpc, struct capref cap) {
    rpc->channel.lmp.remote_cap = cap;
}


void handle_spawn(struct aos_rpc *old_rpc, const char *name, uintptr_t core_id, uintptr_t *new_pid) {
    grading_rpc_handler_process_spawn((char *) /* why no const? :( */ name, core_id);
    uintptr_t current_core_id = disp_get_core_id();
    if(core_id == current_core_id) {
        domainid_t pid;
        errval_t err = spawn_new_domain(name, 0, NULL, &pid, NULL_CAP, NULL_CAP, NULL_CAP, NULL);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "Failed to spawn new domain\n");
        }
        *new_pid = pid;
    } else if (current_core_id != 0) {
        // ump to 0
        errval_t err;
        struct aos_rpc* ump_chan = get_core_channel(0);
        assert(ump_chan && "NO U!");
        err = aos_rpc_call(ump_chan, INIT_IFACE_SPAWN, name, core_id, new_pid);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to call aos rpc in spawn handler for foreign core\n");
        }
    } else { // is 0
        // ump call to core_id
        //OR init distribute on creation of ump channel to all known channels
        // debug_printf("spawn on core id: %d\n", core_id);
        errval_t err;
        struct aos_rpc* ump_chan = get_core_channel(core_id);
        
        if (ump_chan == NULL) {
            debug_printf("can't spawn on core %d: no channel to core\n", core_id);
            *new_pid = 0;
            return;
        }
        err = aos_rpc_call(ump_chan, INIT_IFACE_SPAWN, name, core_id, new_pid);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to call aos rpc in spawn handler for foreign core\n");
        }
        
    }
}


void handle_spawn_extended(struct aos_rpc *rpc, struct aos_rpc_varbytes request, uintptr_t core_id, struct capref spawner_ep,
                                struct capref stdout_cap, struct capref stdin_cap, uintptr_t *new_pid)
{
    coreid_t current_core_id = disp_get_core_id();
    if (core_id != current_core_id) {
        if (current_core_id != 0) {
            // TODO
        }
        else {
            struct aos_rpc* core_rpc = get_core_channel(core_id);
            if (core_rpc == NULL) {
                *new_pid = COREID_INVALID;
                return;
            }

            aos_rpc_call(core_rpc, INIT_IFACE_SPAWN_EXTENDED, request, core_id, spawner_ep, stdout_cap, stdin_cap, new_pid);
            return;
        }
    }


    struct spawn_request_header *header = (struct spawn_request_header *) request.bytes;
    int argc = header->argc;
    char **argv = malloc(argc * sizeof(char *));
    memset(argv, 0, argc * sizeof(char *));

    size_t offset = sizeof(struct spawn_request_header);

    for (int i = 0; i < argc; i++) {
        struct spawn_request_arg *arg_hdr = (struct spawn_request_arg *) (request.bytes + offset);
        argv[i] = malloc(arg_hdr->length);
        memcpy(argv[i], arg_hdr->str, arg_hdr->length);
        offset += sizeof(struct spawn_request_arg) + arg_hdr->length;
    }

    if (argc <= 0) {
        debug_printf("error, no name supplied\n");
        return;
    }

    const char *name = argv[0];

    domainid_t pid;
    errval_t err = spawn_new_domain(name, argc, argv, &pid, spawner_ep, stdout_cap, stdin_cap, NULL);
    if (err_is_fail(err)) {
        *new_pid = MOD_NOT_FOUND;
        return;
    }
    *new_pid = pid;
}


void handle_ns_on(struct aos_rpc *r){
    set_ns_online();
}


//TODO get rid
void handle_service_on(struct aos_rpc *r, uintptr_t service){
    debug_printf("Handle nameserver online\n");
}

void handle_foreign_spawn(struct aos_rpc *origin_rpc, const char *name, uintptr_t core_id, uintptr_t *new_pid)
{   



    // debug_printf("WE SPAWN: %s, %ld\n", name, core_id);
    // struct spawninfo *si = spawn_create_spawninfo();

    // struct aos_rpc *rpc = &si->rpc;
    // //aos_rpc_init(rpc); // TODO (RPC): set interface
    // //initialize_rpc_handlers(rpc);

    // domainid_t *pid = &si->pid;
    // spawn_load_by_name((char*) name, si, pid);
    // *new_pid = *pid;
    // errval_t errr = lmp_chan_register_recv(&rpc->channel.lmp, get_default_waitset(), MKCLOSURE(&aos_rpc_on_lmp_message, &rpc));
    // if (err_is_fail(errr) && errr == LIB_ERR_CHAN_ALREADY_REGISTERED) {
    //     // not too bad, already registered
    // }
}


// /**
//  * \brief handler function for send number rpc call
//  */
// void handle_send_number(struct aos_rpc *r, uintptr_t number) {
//     debug_printf("recieved number: %ld\n", number);
// }

// /**
//  * \brief handler function for send string rpc call
//  */
// void handle_send_string(struct aos_rpc *r, const char *string) {
//     debug_printf("recieved string: %s\n", string);
// }



void handle_init_process_register(struct aos_rpc *r,uintptr_t core_id,const char* name, uintptr_t* pid){
    errval_t err;
    if(disp_get_current_core_id() == 0){
        // debug_printf("Handling proces register in bsp_init\n");
        if (!get_pm_online()) {
            debug_printf("pm not yet online, we will now probably fail\n");
        }
        err = aos_rpc_call(get_pm_rpc(),AOS_RPC_REGISTER_PROCESS,core_id,name,pid);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to forward process registering to process manager in bsp init\n");
        }
    }
    else {
        err = aos_rpc_call(get_core_channel(0),AOS_RPC_REGISTER_PROCESS,disp_get_current_core_id(),name,pid);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to forward process registering to process manager in bsp init\n");
        }
    }
}





void handle_init_get_proc_name(struct aos_rpc *r, uintptr_t pid, char *name){
    errval_t err; 

    char buffer[512];
    if(disp_get_current_core_id() == 0){
        err = aos_rpc_call(get_pm_rpc(),AOS_RPC_GET_PROC_NAME,pid,buffer);

        strcpy(name,buffer);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to forward process registering to process manager in bsp init\n");
        }
    }else if (disp_get_core_id() != 0){
        err = aos_rpc_call(get_core_channel(0),AOS_RPC_GET_PROC_NAME,pid,buffer);
        
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to forward process registering to process manager in app init\n");
        }
    }
    strcpy(name,buffer);
}


void handle_init_get_proc_list(struct aos_rpc *r, uintptr_t *pid_count, char *list){

    errval_t err;
    char buffer[2048]; //TODO: 
    if(disp_get_current_core_id() == 0){
        if (!get_pm_online()) {
            *pid_count = 0;
            *list = 0;
            return;
        }

        err = aos_rpc_call(get_pm_rpc(),AOS_RPC_GET_PROC_LIST,pid_count,&buffer);        
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to forward process registering to process manager in bsp init\n");
        }
    }else if (disp_get_core_id() != 0){
         err = aos_rpc_call(get_core_channel(0),AOS_RPC_GET_PROC_LIST,pid_count,&buffer);
        
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to forward process registering to process manager in app init\n");
        }
        
    }
    strcpy(list,buffer);
}


void handle_init_get_core_id(struct aos_rpc *r, uintptr_t pid, uintptr_t * core_id){
    errval_t err;
    if(disp_get_current_core_id() == 0){
        err = aos_rpc_call(get_pm_rpc(),AOS_RPC_GET_PROC_CORE,pid,core_id);        
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to forward process registering to process manager in bsp init\n");
        }
    }else if (disp_get_core_id() != 0){
         err = aos_rpc_call(get_core_channel(0),AOS_RPC_GET_PROC_CORE,pid,core_id);
        
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to forward process registering to process manager in app init\n");
        }
    }
}





void handle_forward_ns_reg(struct aos_rpc *rpc,uintptr_t core_id,const char* name,struct capref proc_ep_cap, uintptr_t pid, struct capref* ns_ep_cap){

    errval_t err = aos_rpc_call(get_ns_forw_rpc(),INIT_REG_NAMESERVER,core_id,name,proc_ep_cap,pid,ns_ep_cap);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to forward ns reg!\n");
    }
}







void handle_multi_hop_init(struct aos_rpc *rpc,const char* name, struct capref server_ep_cap, struct capref* init_ep_cap){
    errval_t err;



    struct routing_entry * re = (struct routing_entry *) malloc(sizeof(struct routing_entry));
    assert(re && "Routing entry failed!\n");
    strcpy(re -> name,(char *) name);


    struct aos_rpc * new_rpc = malloc(sizeof(struct aos_rpc));

    struct capref new_init_ep_cap;
    err = slot_alloc(&new_init_ep_cap);    
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to slot alloc in multihop init!\n");
    }
    struct lmp_endpoint * lmp_ep;
    err = endpoint_create(256,&new_init_ep_cap,&lmp_ep);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to create ep for multihop\n");
    }
    err = aos_rpc_init_lmp(new_rpc,new_init_ep_cap,server_ep_cap,lmp_ep, get_default_waitset());
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to register waitset on rpc\n");
    }

    err = aos_rpc_set_interface(new_rpc,get_opaque_server_interface(),OS_IFACE_N_FUNCTIONS,get_opaque_server_rpc_handlers);

    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to set server interface in multi hop init\n");
    }
    re -> rpc = new_rpc;
    err = add_routing_entry(re);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to add routing entry for multihop!\n");
    }
    *init_ep_cap = new_init_ep_cap;
}



void handle_client_call(struct aos_rpc *rpc,coreid_t core_id,const char* name,struct aos_rpc_varbytes message,struct capref send_cap,struct aos_rpc_varbytes* response, struct capref *recv_cap, uintptr_t* response_size){
    errval_t err;
    coreid_t curr_core = disp_get_core_id();
    if(core_id != curr_core){
        struct aos_rpc* fw_rpc;
        if(curr_core == 0){ fw_rpc = get_core_channel(core_id);}
        else{fw_rpc = get_core_channel(0);}
        assert(fw_rpc && "Core channel not online!");
        err = aos_rpc_call(fw_rpc,INIT_CLIENT_CALL,core_id,name,message,send_cap,response,recv_cap, response_size);
        if(err_is_fail(err)){DEBUG_ERR(err,"Failed forward!");}
    }else {



        struct routing_entry * re;
        err = get_routing_entry_by_name(name,&re);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to get routing entry by name\n");
            return;
        }


        err = aos_rpc_call(re -> rpc,OS_IFACE_MESSAGE,message,send_cap,response,recv_cap,response_size);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed call to server ep!\n");
        }
    }
}



void handle_client_call1(struct aos_rpc *rpc,coreid_t core_id,const char* name,struct aos_rpc_varbytes message,struct capref send_cap,struct aos_rpc_varbytes* response, uintptr_t* response_size){
    // debug_printf("handling client call! 1\n");
    errval_t err;
    coreid_t curr_core = disp_get_core_id();
    if(core_id != curr_core){
        struct aos_rpc* fw_rpc;
        if(curr_core == 0){ fw_rpc = get_core_channel(core_id);}
        else{fw_rpc = get_core_channel(0);}
        assert(fw_rpc && "Core channel not online!");
        err = aos_rpc_call(fw_rpc,INIT_CLIENT_CALL1,core_id,name,message,send_cap,response,response_size);
        if(err_is_fail(err)){DEBUG_ERR(err,"Failed forward!");}
    }else {


        struct routing_entry * re;
        err = get_routing_entry_by_name(name,&re);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to get routing entry by name\n");
            return;
        }

        struct capref dummy_cap;
        err = aos_rpc_call(re -> rpc,OS_IFACE_MESSAGE,message,send_cap,response,&dummy_cap,response_size);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed call to server ep!\n");
        }
    }
}



void handle_client_call2(struct aos_rpc *rpc,coreid_t core_id,const char* name,struct aos_rpc_varbytes message,struct aos_rpc_varbytes* response, uintptr_t* response_size){


    errval_t err;
    coreid_t curr_core = disp_get_core_id();
    if(core_id != curr_core){
        struct aos_rpc* fw_rpc;
        if(curr_core == 0){ fw_rpc = get_core_channel(core_id);}
        else{fw_rpc = get_core_channel(0);}
        assert(fw_rpc && "Core channel not online!");
        err = aos_rpc_call(fw_rpc,INIT_CLIENT_CALL2,core_id,name,message,response,response_size);
        if(err_is_fail(err)){DEBUG_ERR(err,"Failed forward!");}
    }else {


        struct routing_entry * re;
        err = get_routing_entry_by_name(name,&re);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to get routing entry by name\n");
            return;
        }
        struct capref dummy_cap;
        err = aos_rpc_call(re -> rpc,OS_IFACE_MESSAGE,message,NULL_CAP,response,&dummy_cap,response_size);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed call to server ep!\n");
        }
    }
}


void handle_client_call3(struct aos_rpc *rpc,coreid_t core_id,const char* name,struct aos_rpc_varbytes message, struct aos_rpc_varbytes* response, struct capref *recv_cap, uintptr_t* response_size){
    errval_t err;
    coreid_t curr_core = disp_get_core_id();
    if(core_id != curr_core){
        struct aos_rpc* fw_rpc;
        if(curr_core == 0){ fw_rpc = get_core_channel(core_id);}
        else{fw_rpc = get_core_channel(0);}
        assert(fw_rpc && "Core channel not online!");
        err = aos_rpc_call(fw_rpc,INIT_CLIENT_CALL3,core_id,name,message,response,recv_cap,response_size);
        if(err_is_fail(err)){DEBUG_ERR(err,"Failed forward!");}
    }else {

        struct routing_entry * re;
        err = get_routing_entry_by_name(name,&re);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to get routing entry by name\n");
            return;
        }
        err = aos_rpc_call(re -> rpc,OS_IFACE_MESSAGE,message,NULL_CAP,response,recv_cap,response_size);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed call to server ep!\n");
        }
    }
}


void handle_binding_request(struct aos_rpc * rpc,const char* name,uintptr_t src_core,uintptr_t target_core,struct capref client_ep_cap, struct capref * server_ep_cap){
    errval_t err;
    if(disp_get_core_id() == target_core){
        struct routing_entry* re;
        err = get_routing_entry_by_name(name, &re);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to get routing entry in forwarding of binding request!\n");
        }

        err = aos_rpc_call(re -> rpc,OS_IFACE_BINDING_REQUEST,src_core,client_ep_cap,server_ep_cap);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to forward to server listener\n");
        }
    }else{
        struct aos_rpc * next_hop;
        if(disp_get_core_id() == 0){
            next_hop = get_core_channel(target_core);
        }else{
            next_hop = get_core_channel(0);

        }
        err = aos_rpc_call(next_hop,INIT_BINDING_REQUEST,name,src_core,target_core,client_ep_cap,server_ep_cap);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to forward!\n");
        }
    }
}


void handle_get_all_modules(struct aos_rpc *rpc, char* modules)
{
    memset(modules, 0, 1024);
    for(size_t i = 0; i < bi->regions_length; i++) {
        struct mem_region *region = &bi->regions[i];
        const char *modname = multiboot_module_name(region);
        if (modname != NULL) {
            char *only_name = strrchr(modname, '/') + 1;
            strcat(modules, only_name);
            strcat(modules, ";");
        }
    }
}


/**
 * \brief initialize all handlers for rpc calls
 * 
 * Init needs to provide a bunch of rpc interfaces to other processes.
 * This function is calld each time a new process is spawned and makes sure
 * that all necessary rpc handlers in si->rpc are set. It then registers
 * the lmp recieve handler.
 */
errval_t initialize_rpc_handlers(struct aos_rpc *rpc)
{   

    //STANDARD INTERFACE (MOSTLY FOR TESTS + MEMEATER)
    void handle_roundtrip(struct aos_rpc *r) { return; }
    aos_rpc_register_handler(rpc, AOS_RPC_INITIATE, &handle_initiate);
    aos_rpc_register_handler(rpc, AOS_RPC_SEND_NUMBER, &handle_send_number);
    aos_rpc_register_handler(rpc, AOS_RPC_SEND_STRING, &handle_send_string);
    aos_rpc_register_handler(rpc, AOS_RPC_PUTCHAR, &handle_putchar);
    aos_rpc_register_handler(rpc, AOS_RPC_GETCHAR, &handle_getchar);
    aos_rpc_register_handler(rpc, AOS_RPC_ROUNDTRIP, &handle_roundtrip);


    //INIT INTERFACE (MOSTLY FORWARDING)
    aos_rpc_register_handler(rpc, INIT_IFACE_SPAWN, &handle_spawn);
    aos_rpc_register_handler(rpc, INIT_IFACE_SPAWN_EXTENDED, &handle_spawn_extended);
    aos_rpc_register_handler(rpc, INIT_NAMESERVER_ON, &handle_ns_on);
    aos_rpc_register_handler(rpc,INIT_REG_NAMESERVER,&handle_forward_ns_reg);
    aos_rpc_register_handler(rpc,INIT_MULTI_HOP_CON,&handle_multi_hop_init);
    aos_rpc_register_handler(rpc,INIT_CLIENT_CALL,&handle_client_call);
    aos_rpc_register_handler(rpc,INIT_CLIENT_CALL1,&handle_client_call1);
    aos_rpc_register_handler(rpc,INIT_CLIENT_CALL2,&handle_client_call2);
    aos_rpc_register_handler(rpc,INIT_CLIENT_CALL3,&handle_client_call3);
    aos_rpc_register_handler(rpc,INIT_BINDING_REQUEST,&handle_binding_request);
    aos_rpc_register_handler(rpc, INIT_IFACE_GET_ALL_MODULES, &handle_get_all_modules);
    aos_rpc_register_handler(rpc,INIT_FS_ON,&handle_fs_on);

    return SYS_ERR_OK;
}



void register_core_channel_handlers(struct aos_rpc *rpc)
{
    aos_rpc_register_handler(rpc, AOS_RPC_SEND_NUMBER, &handle_send_number);
    aos_rpc_register_handler(rpc, AOS_RPC_SEND_STRING, &handle_send_string);
    aos_rpc_register_handler(rpc, AOS_RPC_FOREIGN_SPAWN, &handle_foreign_spawn);
    aos_rpc_register_handler(rpc, AOS_RPC_REQUEST_RAM, &handle_request_ram);

    aos_rpc_register_handler(rpc, AOS_RPC_REGISTER_PROCESS, &handle_init_process_register);
    aos_rpc_register_handler(rpc,AOS_RPC_GET_PROC_NAME,&handle_init_get_proc_name);
    aos_rpc_register_handler(rpc,AOS_RPC_GET_PROC_LIST,&handle_init_get_proc_list);
    aos_rpc_register_handler(rpc, AOS_RPC_GET_PROC_CORE,&handle_init_get_core_id);
    
}


void handle_fs_on(struct aos_rpc *rpc){
    set_fs_online();
}