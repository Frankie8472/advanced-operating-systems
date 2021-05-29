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
    
}

void initialize_nameservice_handlers(struct aos_rpc *ns_rpc){
    aos_rpc_register_handler(ns_rpc,NS_GET_PID,&handle_pid_request);
    aos_rpc_register_handler(ns_rpc,NS_GET_PROC_NAME,&handle_get_proc_name);
    aos_rpc_register_handler(ns_rpc,NS_GET_PROC_CORE,&handle_get_proc_core);
    aos_rpc_register_handler(ns_rpc,NS_GET_PROC_LIST,&handle_get_proc_list);
    aos_rpc_register_handler(ns_rpc,NS_DEREG_PROCESS,&handle_dereg_process);

    aos_rpc_register_handler(ns_rpc,NS_DEREG_SERVER,&handle_dereg_server);
    aos_rpc_register_handler(ns_rpc,NS_ENUM_SERVERS,&handle_enum_servers);
    aos_rpc_register_handler(ns_rpc,NS_NAME_LOOKUP,&handle_server_lookup);
    aos_rpc_register_handler(ns_rpc,NS_LOOKUP_PROP,&handle_server_lookup_with_prop);
    aos_rpc_register_handler(ns_rpc,NS_GET_SERVER_PROPS,&handle_get_props);
    aos_rpc_register_handler(ns_rpc,NS_LIVENESS_CHECK,&handle_liveness_check);
    aos_rpc_register_handler(ns_rpc,NS_REG_SERVER,&handle_reg_server);
}

void handle_server_lookup(struct aos_rpc *rpc, char *name,uintptr_t* core_id,uintptr_t *direct,uintptr_t * success){
    debug_printf("Handling server lookup\n");
    errval_t err;
    struct server_list* server;
    err = find_server_by_name(name,&server);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to find server: %s \n",name);
        *success = 0;
    }else{
        *core_id =  server -> core_id;
        *direct  = server -> direct;
        *success = 1;
    }
}

void handle_server_lookup_with_prop(struct aos_rpc *rpc, char *query,uintptr_t* core_id,uintptr_t *direct,uintptr_t * success, char * response_name){
    debug_printf("Handling lookup with props!\n");
    errval_t err;
    char * keys[N_PROPERTIES];
    char * values[N_PROPERTIES];
    char * name;
    size_t prop_size;
    err = deserialize_prop(query,keys,values,&name,&prop_size);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to deserialize sever request\n");
        *success = 0;
        return;
    }


    struct server_list* server;
    err = find_server_by_name_and_property(name,keys,values,prop_size,&server);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to find server with matching query: %s\n",query);
        *success = 0;
        return;
    }
    *direct = server -> direct;
    *core_id = server -> core_id;
    char buffer[SERVER_NAME_SIZE];
    strcpy(buffer,server->name);
    response_name = buffer;
    *success = 1;
    debug_printf("%s\n",response_name);


}

void handle_reg_server(struct aos_rpc * rpc, uintptr_t pid, uintptr_t core_id ,const char* server_data, uintptr_t direct,  char * return_message){
    errval_t err;

    struct server_list * new_server = (struct server_list * ) malloc(sizeof(struct server_list));

    // serve
    char* serv_name = (char *) malloc(SERVER_NAME_SIZE * sizeof(char));
    err  = deserialize_prop(server_data,new_server -> key,new_server -> value,(char**) &serv_name,&new_server -> n_properties);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to deserialize sever request\n");
    }




    new_server -> next = NULL;
    strcpy(new_server -> name,serv_name);
    new_server -> pid = pid; 
    new_server -> core_id = core_id;
    new_server -> direct = direct;
    new_server -> marked = false;
    free(serv_name);



    err = add_server(new_server);
    if(err_is_fail(err)){
        return_message = "Non-unique name!\n";
        DEBUG_ERR(err,"Failed to add new server!\n");
    }

    *return_message = '\0';
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
    // response = (char *) malloc(SERVER_NAME_SIZE * n_servers * sizeof(char)); //enough for all names 
    // debug_printf("Got here 0x%lx!\n",resp_size);
    find_servers_by_prefix(name,response,resp_size);
    // debug_printf("Response : %s\n",response);
}









void handle_get_props(struct aos_rpc *rpc,const char* name, char * response){
    debug_printf("here!\n");
    struct server_list *server;
    errval_t err = find_server_by_name((char*) name,&server);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to find server\n");
        *response = '\0';
        return;
    }
    for(size_t i = 0; i < server -> n_properties;++i){
        strcat(response,server -> key[i]);
        strcat(response,"=");
        strcat(response,server -> value[i]);
        if(i != server -> n_properties - 1){
            strcat(response,",");
        }
    }
    
}

void handle_liveness_check(struct aos_rpc *rpc, const char* name){
    struct server_list *server;
    errval_t err; 
    err = find_server_by_name((char*)name,&server);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to find server: %s\n",name);
    }
    domainid_t check_pid;
    err = find_process_by_rpc(rpc,&check_pid);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to find process by rpc for liveness check\n");
    }
    if(server -> pid == check_pid){
        server -> marked = false;
    }
}