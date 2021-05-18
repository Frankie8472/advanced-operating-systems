#include <stdio.h>
#include <stdlib.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/default_interfaces.h>
#include "nameserver_handlers.h"
#include "process_list.h"
#include "server_list.h"
#include "process_manager_handlers.h"
#include <aos/nameserver.h>

void *name_server_rpc_handlers[NS_IFACE_N_FUNCTIONS];


void initialize_ns_handlers(struct aos_rpc * init_rpc){
    aos_rpc_register_handler(init_rpc,INIT_REG_NAMESERVER,&handle_reg_proc);
    aos_rpc_register_handler(init_rpc,INIT_REG_SERVER,&handle_server_request);
    aos_rpc_register_handler(init_rpc,INIT_NAME_LOOKUP,&handle_server_lookup);
}

void initialize_nameservice_handlers(struct aos_rpc *ns_rpc){
    aos_rpc_register_handler(ns_rpc,NS_GET_PID,&handle_pid_request);
    aos_rpc_register_handler(ns_rpc,NS_GET_PROC_NAME,&handle_get_proc_name);
    aos_rpc_register_handler(ns_rpc,NS_GET_PROC_CORE,&handle_get_proc_core);
    aos_rpc_register_handler(ns_rpc,NS_GET_PROC_LIST,&handle_get_proc_list);
    aos_rpc_register_handler(ns_rpc,NS_DEREG_PROCESS,&handle_dereg_process);

    aos_rpc_register_handler(ns_rpc,NS_DEREG_SERVER,&handle_dereg_server);
    aos_rpc_register_handler(ns_rpc,NS_ENUM_SERVERS,&handle_enum_servers);
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
        *success = 1;
        debug_printf("Server %s already removed!\n",name);
        return;
    }
    if(pid == -1 || pid == ret_server -> pid){ //if process is dead, anyone can deregister server
        remove_server(ret_server);
        *success = 1;
        print_server_list();
    }else{
        *success = 0;
    }

    
}


void handle_enum_servers(struct aos_rpc *rpc,const char* name, char * response, uintptr_t * resp_size){
    response = (char *) malloc(SERVER_NAME_SIZE * n_servers * sizeof(char)); //enough for all names 
    // debug_printf("Got here 0x%lx!\n",resp_size);
    find_servers_by_prefix(name,response,resp_size);
}









