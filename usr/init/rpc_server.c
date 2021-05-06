
#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/coreboot.h>


#include <spawn/spawn.h>

#include <spawn/process_manager.h>
#include <mm/mm.h>
#include "rpc_server.h"
#include "spawn_server.h"
#include "mem_alloc.h"

#include "../../lib/aos/include/init.h"





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

errval_t init_terminal_state(void)
{
    struct terminal_state ts;
    ts.reading = false;
    ts.index = 0;
    ts.waiting = NULL;
    terminal_state = &ts;
    
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
    err = aos_rpc_init(rpc);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_RPC_INIT);
    aos_rpc_init_ump_default(rpc, urpc_frame, BASE_PAGE_SIZE, coreid < disp_get_core_id());
    ON_ERR_PUSH_RETURN(err, LIB_ERR_RPC_INIT);

    register_core_channel_handlers(rpc);


    set_core_channel(coreid,rpc);
    
    return SYS_ERR_OK;
}


/**
 * \brief handler function for putchar rpc call
 */
void handle_putchar(struct aos_rpc *r, uintptr_t c) {
    putchar(c);
    //debug_printf("recieved: %c\n", (char)c);
}

/**
 * \brief handler function for getchar rpc call
 */
void handle_getchar(struct aos_rpc *r, uintptr_t *c) {
    int v = getchar();
    //debug_printf("getchar: %c\n", v);
    *c = v;//getchar();
}

/**
 * \brief handler function for ram alloc rpc call
 */
void handle_request_ram(struct aos_rpc *r, uintptr_t size, uintptr_t alignment, struct capref *cap, uintptr_t *ret_size) {
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
    uintptr_t current_core_id = disp_get_core_id();
    if(core_id == current_core_id) {
        domainid_t pid;
        errval_t err = spawn_new_domain(name, &pid);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "Failed to spawn new domain\n");
        }
        *new_pid = pid;
    } else if (current_core_id != 0) {
        // ump to 0
        errval_t err;
        struct aos_rpc* ump_chan = get_core_channel(0);
        assert(ump_chan && "NO U!");
        err = aos_rpc_call(ump_chan, AOS_RPC_FOREIGN_SPAWN, name, core_id, new_pid);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to call aos rpc in spawn handler for foreign core\n");
        }
    } else { // is 0
        // ump call to core_id
        //OR init distribute on creation of ump channel to all known channels
        debug_printf("spawn on core id: %d\n", core_id);
        errval_t err;
        struct aos_rpc* ump_chan = get_core_channel(core_id);
        
        if (ump_chan == NULL) {
            debug_printf("can't spawn on core %d: no channel to core\n", core_id);
            *new_pid = 0;
            return;
        }
        err = aos_rpc_call(ump_chan, AOS_RPC_FOREIGN_SPAWN, name, core_id, new_pid);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to call aos rpc in spawn handler for foreign core\n");
        }
        
    }
}


void handle_service_on(struct aos_rpc *r, uintptr_t service){
    debug_printf("Handle pm online\n");
    switch(service){
        case PROCESS_MANAGER: 
            set_pm_online();
            break;
        case MEMORY_SERVER:
            set_mem_online();
            break;
        default:
            debug_printf("Invalid parameter to turn on service\n");
    }
}

void handle_foreign_spawn(struct aos_rpc *origin_rpc, const char *name, uintptr_t core_id, uintptr_t *new_pid)
{
    debug_printf("WE SPAWN: %s, %ld\n", name, core_id);
    struct spawninfo *si = spawn_create_spawninfo();

    struct aos_rpc *rpc = &si->rpc;
    aos_rpc_init(rpc);
    initialize_rpc_handlers(rpc);

    domainid_t *pid = &si->pid;
    spawn_load_by_name((char*) name, si, pid);
    *new_pid = *pid;
    errval_t errr = lmp_chan_register_recv(&rpc->channel.lmp, get_default_waitset(), MKCLOSURE(&aos_rpc_on_lmp_message, &rpc));
    if (err_is_fail(errr) && errr == LIB_ERR_CHAN_ALREADY_REGISTERED) {
        // not too bad, already registered
    }
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
        debug_printf("Handling proces register in bsp_init\n");
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


void handle_mem_server_request(struct aos_rpc *r, struct capref client_cap, struct capref * server_cap){
    errval_t err;
    err = aos_rpc_call(get_mem_rpc(),AOS_RPC_MEM_SERVER_REQ,client_cap,server_cap);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to relay memory server request\n");
    }
}


void handle_init_get_proc_name(struct aos_rpc *r, uintptr_t pid, char *name){
    errval_t err; 

    char buffer[512];
    if(disp_get_current_core_id() == 0){
        err = aos_rpc_call(get_pm_rpc(),AOS_RPC_GET_PROC_NAME,pid,buffer);
        // debug_printf("String received : %s\n",buffer);

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
    // debug_printf("Handled init get proc list %d, %s\n");
    // debug_printf("pid count : %lx, list: %lx\n",pid_count,list);
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


void handle_all_binding_request(struct aos_rpc *r, uintptr_t pid, uintptr_t core_id,uintptr_t client_core,struct capref client_cap,struct capref * server_cap){

    errval_t err; 
    // debug_printf("Forwarding binding request to pid: %d\n",pid);
    
    
    if(pid == disp_get_domain_id()){
        // init channel 
        assert(NULL && "shoudnt get here");

        if(core_id !=  disp_get_current_core_id()){
            //init ump
        }
        else {
            // debug_printf("Handling binding request\n");
            struct lmp_endpoint * lmp_ep;
            struct capref self_ep_cap = (struct capref) {
                .cnode = cnode_task,
                .slot = TASKCN_SLOT_SELFEP
            };

            err = endpoint_create(256,&self_ep_cap,&lmp_ep);
            if(err_is_fail(err)){
                DEBUG_ERR(err,"Failed to create ep in binding request\n");
            }

            static struct aos_rpc rpc;
            err = aos_rpc_init(&rpc);
            if(err_is_fail(err)){
                DEBUG_ERR(err,"Failed to init rpc in binding request \n");
            }

            err = aos_rpc_init_lmp(&rpc,self_ep_cap,client_cap,lmp_ep);
            if(err_is_fail(err)){
                DEBUG_ERR(err,"Failed to register waitset on rpc\n");
            }

            assert(NULL && "shoudnt get here");
            *server_cap = self_ep_cap;
        }

    }else {
        if(core_id == disp_get_current_core_id()){ // this means we are on init process of same core as server
            struct aos_rpc* target_server_rpc = get_rpc_from_spawn_info(pid);
            assert(target_server_rpc && "Failed to find the target server rpc in handle binding request\n");
            err = aos_rpc_call(target_server_rpc,AOS_RPC_BINDING_REQUEST,pid, core_id,client_core, client_cap,server_cap);
            if(err_is_fail(err)){
                DEBUG_ERR(err, "Failed to send from init to target server rpc!\n");
            }

            // char buf[512];
            // debug_print_cap_at_capref(buf,512,*server_cap);
            // debug_printf("Cap her in init %s\n",buf);


        }else if(get_init_domain() && core_id == 0){//forward to init on core core_id
            err = aos_rpc_call(get_core_channel(core_id), AOS_RPC_BINDING_REQUEST,pid,core_id,client_core,client_cap,server_cap);
            if(err_is_fail(err)){
                DEBUG_ERR(err,"FAiled to forward from core 0 to core %d in handle binding request",core_id);
            }
        }
        else{ // this means we are on init and need to forward to init on cor 0
            assert(get_init_domain() && "Shouldnt get here, something wrong in the binding request rpc");
            err = aos_rpc_call(get_core_channel(0), AOS_RPC_BINDING_REQUEST,pid,core_id,client_core,client_cap,server_cap);
            if(err_is_fail(err)){
                DEBUG_ERR(err,"Failed to forward from init to init on core 0\n");
            }
        }


       
       
    }

     if(client_core != core_id){
        *server_cap  = client_cap; //NOTE: this is fucking stupid
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
    aos_rpc_register_handler(rpc,AOS_RPC_SERVICE_ON,&handle_service_on);
    aos_rpc_register_handler(rpc, AOS_RPC_INITIATE, &handle_initiate);
    aos_rpc_register_handler(rpc, AOS_RPC_SEND_NUMBER, &handle_send_number);
    aos_rpc_register_handler(rpc, AOS_RPC_SEND_STRING, &handle_send_string);

    aos_rpc_register_handler(rpc, AOS_RPC_REQUEST_RAM, &handle_request_ram);

    aos_rpc_register_handler(rpc, AOS_RPC_PROC_SPAWN_REQUEST, &handle_spawn);

    aos_rpc_register_handler(rpc, AOS_RPC_PUTCHAR, &handle_putchar);
    aos_rpc_register_handler(rpc, AOS_RPC_GETCHAR, &handle_getchar);

    void handle_roundtrip(struct aos_rpc *r) { return; }
    aos_rpc_register_handler(rpc, AOS_RPC_ROUNDTRIP, &handle_roundtrip);
    aos_rpc_register_handler(rpc,AOS_RPC_REGISTER_PROCESS,&handle_init_process_register);
    aos_rpc_register_handler(rpc,AOS_RPC_MEM_SERVER_REQ,&handle_mem_server_request);

    aos_rpc_register_handler(rpc,AOS_RPC_GET_PROC_NAME,&handle_init_get_proc_name);
    aos_rpc_register_handler(rpc,AOS_RPC_GET_PROC_LIST,&handle_init_get_proc_list);
    aos_rpc_register_handler(rpc, AOS_RPC_GET_PROC_CORE,&handle_init_get_core_id);
    aos_rpc_register_handler(rpc,AOS_RPC_BINDING_REQUEST,&handle_all_binding_request);
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
    aos_rpc_register_handler(rpc,AOS_RPC_BINDING_REQUEST,&handle_all_binding_request );

    void handle_roundtrip(struct aos_rpc *r) { return; }
    aos_rpc_register_handler(rpc, AOS_RPC_ROUNDTRIP, &handle_roundtrip);
}


