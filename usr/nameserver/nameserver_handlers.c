#include <stdio.h>
#include <stdlib.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/default_interfaces.h>
#include "nameserver_handlers.h"
#include "process_list.h"
#include "server_list.h"
#include <aos/nameserver.h>

// extern struct process_list pl;
static void *name_server_rpc_handlers[NS_IFACE_N_FUNCTIONS];

void initialize_ns_handlers(struct aos_rpc * init_rpc){
    aos_rpc_register_handler(init_rpc,INIT_REG_NAMESERVER,&handle_reg_proc);
    aos_rpc_register_handler(init_rpc,INIT_REG_SERVER,&handle_server_request);
    aos_rpc_register_handler(init_rpc,INIT_NAME_LOOKUP,&handle_server_lookup);
}

void initialize_nameservice_handlers(struct aos_rpc *ns_rpc){
    aos_rpc_register_handler(ns_rpc,NS_GET_PROC_NAME,&handle_get_proc_name);
    aos_rpc_register_handler(ns_rpc,NS_GET_PROC_CORE,&handle_get_proc_core);
    aos_rpc_register_handler(ns_rpc,NS_GET_PROC_LIST,&handle_get_proc_list);
    aos_rpc_register_handler(ns_rpc,NS_GET_PID,&handle_pid_request);
    aos_rpc_register_handler(ns_rpc,NS_DEREG_SERVER,&handle_dereg_server);

}

void handle_server_lookup(struct aos_rpc *rpc, char *name,uintptr_t* core_id,uintptr_t *ump,struct capref* server_ep_cap){
    debug_printf("Handling server lookup\n");
    errval_t err;
    struct server_list* server;
    err = find_server_by_name(name,&server);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to find server: %s \n",name);
        *server_ep_cap = NULL_CAP;
        return;
    }

    char buf[512];
    debug_print_cap_at_capref(buf,512,server -> end_point);
    debug_printf("|| P: %d | C: %d | N: %s | UMP: %d | EP: %s \n", server -> pid, server -> core_id, server -> name,server -> ump,buf);

    *core_id =  server -> core_id;
    *ump  = server -> ump;
    *server_ep_cap = server -> end_point;

}

void handle_server_request(struct aos_rpc * rpc, uintptr_t pid, uintptr_t core_id ,const char* server_data, struct capref server_ep_cap, const char * return_message){
    errval_t err;

    struct server_list * new_server = (struct server_list * ) malloc(sizeof(struct server_list));
    // struct capref new_server_ep_cap;

    // serve
    const char* serv_name;
    err  = deserialize_prop(server_data,new_server -> key,new_server -> value,(char**)&serv_name);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to deserialize sever request\n");
    }


    if(!verify_name(serv_name)){
        return_message = "Invalid name!\n";
        free(new_server);
        return;
    }
    // err = find_server_by_name(new_server,)
    struct capability cap;
    err = invoke_cap_identify(server_ep_cap,&cap);
    // ON_ERR_RETURN(err);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to invoke cap identify on receiving server request!\n");
    }
    bool ump = false;
    if(cap.type == ObjType_Frame){
        ump = true;
    }

    new_server -> next = NULL;
    new_server -> name = serv_name;
    new_server -> pid = pid; 
    new_server -> core_id = core_id;
    // new_server -> end_point = &new_server_ep_cap;
    new_server -> ump = ump;



    err = slot_alloc(&new_server -> end_point);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to slot alloc in handler server request\n");
    }
    err = cap_copy(new_server -> end_point,server_ep_cap);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Faild cap copy in server request handler\n");
    }

    err = add_server(new_server);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to add new server!\n");
    }

    print_server_list();
}

void handle_reg_proc(struct aos_rpc *rpc,uintptr_t core_id,const char* name,struct capref proc_ep_cap, uintptr_t pid, struct capref* ns_ep_cap){

    


    errval_t err;
    struct aos_rpc* new_rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
    err = add_process(core_id,name,(domainid_t )pid,new_rpc);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to add process to process list\n");
    }

    if(core_id == disp_get_core_id()){ //create_lmp_channel
        struct capref new_ep_cap;
        err = slot_alloc(&new_ep_cap);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed slot alloc in nameserver!\n");
        }

        struct lmp_endpoint * lmp_ep;
        err = endpoint_create(256,&new_ep_cap,&lmp_ep);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to create ep in nameserver server\n");
        }
        err = aos_rpc_init_lmp(new_rpc,new_ep_cap,proc_ep_cap,lmp_ep, get_default_waitset());
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to register waitset on nameserver rpc\n");
        }
        *ns_ep_cap = new_ep_cap;
    }else{ //create_ump_channel
        char *urpc_data = NULL;
        err = paging_map_frame_complete(get_current_paging_state(), (void **) &urpc_data, proc_ep_cap, NULL, NULL);
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to map urpc frame for ump channel into virtual address space\n");
        }
        err =  aos_rpc_init_ump_default(new_rpc,(lvaddr_t) urpc_data, BASE_PAGE_SIZE,false);//take second half as nameserver
        if(err_is_fail(err)){
            DEBUG_ERR(err,"Failed to init_ump_default\n");
        } 
        *ns_ep_cap = proc_ep_cap; //NOTE: this is fucking stupid
    }

    err = aos_rpc_set_interface(new_rpc,get_nameserver_interface(),NS_IFACE_N_FUNCTIONS,name_server_rpc_handlers);
    initialize_nameservice_handlers(new_rpc);
}



void handle_dereg_server(struct aos_rpc *rpc, const char* name, uintptr_t* success){
    errval_t err;
    domainid_t pid;
    err = find_process_by_rpc(rpc,&pid);
    if(err_is_fail(err)){
        debug_printf("Trying to delete server from nonexistent process\n"); 
    
    }
    struct server_list * ret_server;
    err = find_server_by_name((char*) name,&ret_server);
    if(err_is_fail(err)){
        debug_printf("Server %s already removed!\n",name);
        return;
    }
    if(pid == -1 || pid == ret_server -> pid){ //if process is dead, anyone can deregister server
        remove_server(ret_server);
        print_server_list();
    }else{
        debug_printf("Invalid access rights to delete server!\n");
    }

    
}


void handle_get_proc_name(struct aos_rpc *rpc, uintptr_t pid,char* name){
    for(struct process * curr = pl.head; curr != NULL; curr = curr -> next){
        if(curr -> pid == pid){
            size_t n = strlen(curr -> name) + 1;
            for(int i = 0; i < n;++i ){
                name[i] = curr -> name[i];
            }
            // debug_printf("sending: %s\n",name);
            return;
        }
    }
    debug_printf("could not resolve pid name lookup!\n");
}


void handle_get_proc_core(struct aos_rpc* rpc, uintptr_t pid,uintptr_t *core){

    errval_t err =  get_core_id(pid,(coreid_t*)core);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to find core id form domain %d\n",pid);
        *core = -1;
    }
}



void handle_get_proc_list(struct aos_rpc *rpc, uintptr_t *size,char * pids){
    // debug_printf("Handle get list of processes\n");
    *size = pl.size;
    char buffer[12]; 
    size_t index = 0;
    for(struct process * curr = pl.head; curr != NULL; curr = curr -> next){
        sprintf(buffer,"%d",curr -> pid);
        // debug_printf("here is buf %s", buffer);
        char * b_ptr = buffer;
        while(*b_ptr != '\0'){
            pids[index] =  *b_ptr;
            index++;
            b_ptr++;
            if(index > 1021){
                debug_printf("Buffer in channels is not large enough to sned full pid list!\n");
                pids[index] = '\0';
                return;
            }
        }

        pids[index] = ',';
        index++;
    }

    pids[index] = '\0';
    // debug_printf("%s\n",pids);
}


void handle_pid_request(struct aos_rpc *rpc,uintptr_t* pid){
    *pid = process++;
}