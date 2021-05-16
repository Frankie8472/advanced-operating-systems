#include "server_list.h"






errval_t add_server(domainid_t pid, coreid_t core_id,const char* name, struct capref end_point,const char* properties){
    errval_t err;
    struct server_list * new_server = (struct server_list * ) malloc(sizeof(struct server_list));

    struct capability cap;
    err = invoke_cap_identify(end_point,&cap);
    ON_ERR_RETURN(err);
    bool ump = false;
    if(cap.type == ObjType_Frame){
        ump = true;
    }

    new_server -> next = NULL;
    new_server -> name = name;
    new_server -> pid = pid; 
    new_server -> core_id = core_id;
    new_server -> end_point = &end_point;
    new_server -> ump = ump;
    new_server -> properties = properties;
    if(servers == NULL){
        servers = new_server;
    }else {
        struct server_list* curr = servers;
        for(;curr -> next != NULL;curr = curr -> next){}
        curr -> next = new_server;
    }
    return SYS_ERR_OK;
}


errval_t find_server_by_name(char * name, struct server_list * ret_serv){
    struct server_list* curr = servers;
    for(;curr != NULL;curr = curr -> next){
        if(!strcmp(name,curr -> name)){
            ret_serv = curr;
            return SYS_ERR_OK;
        }
    }
    return PROC_MGMT_ERR_DOMAIN_NOT_RUNNING;
}

bool verify_name(const char* name){
    return true;
}

void print_server_list(void){
    debug_printf("================ Servers ==============================\n");
    for(struct server_list * curr = servers; curr != NULL; curr = curr -> next){
        char buffer[512];
        debug_print_cap_at_capref(buffer,512,*curr ->end_point);
        debug_printf("|| P: %d | C: %d | N: %s | UMP: %d | EP: %s                \n", curr -> pid, curr -> core_id, curr -> name,curr -> ump, buffer);
        //TODO print properties
    }


    debug_printf("=======================================================\n");
}