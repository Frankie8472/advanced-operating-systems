#include "server_list.h"
#include "process_list.h"
#include <hashtable/hashtable.h>


#include <aos/nameserver.h>



// errval_t add_server(domainid_t pid, coreid_t core_id,const char* name, struct capref end_point,const char* properties){
//     errval_t err;
//     struct server_list * new_server = (struct server_list * ) malloc(sizeof(struct server_list));

//     struct capability cap;
//     err = invoke_cap_identify(end_point,&cap);
//     ON_ERR_RETURN(err);
//     bool direct = false;
//     if(cap.type == ObjType_Frame){
//         direct = true;
//     }

//     debug_printf("Register with name : %s\n",name);

//     new_server -> next = NULL;
//     new_server -> name = name;
//     new_server -> pid = pid; 
//     new_server -> core_id = core_id;
//     new_server -> end_point = &end_point;
//     new_server -> direct = direct;
//     new_server -> properties = properties;
//     if(servers == NULL){
//         servers = new_server;
//     }else {
//         struct server_list* curr = servers;
//         for(;curr -> next != NULL;curr = curr -> next){}
//         curr -> next = new_server;
//     }
//     return SYS_ERR_OK;
// }


errval_t add_server(struct server_list* new_server){
    struct server_list* curr = servers;
    if(curr == NULL){
        servers = new_server;
    }
    else {
        
        for(;curr -> next != NULL;curr = curr -> next){
            debug_printf("%s =? %s\n",new_server->name, curr -> name);
            if(!strcmp(new_server-> name, curr -> name)){
                free(new_server);
                return LIB_ERR_NAMESERVICE_INVALID_REGISTER;
            }
        }

        if(!strcmp(new_server-> name, curr -> name)){
            free(new_server);
            return LIB_ERR_NAMESERVICE_INVALID_REGISTER;
        }
        curr -> next = new_server;
    }
    n_servers++;


    int failed = server_ht -> d.put_word(&server_ht ->d,new_server -> name,strlen(new_server -> name),(uintptr_t) new_server);
    if(failed){
        return LIB_ERR_NAMESERVICE_HASHTABLE_ERROR;
    }
    return SYS_ERR_OK;
}

errval_t find_server_by_name(char * name, struct server_list ** ret_serv){
    // struct server_list* curr = servers;
    // for(;curr != NULL;curr = curr -> next){
    //     if(!strcmp(name,curr -> name)){
    //         *ret_serv = curr;
    //         return SYS_ERR_OK;
    //     }
    // }
    server_ht -> d.get(&server_ht ->d,name,strlen(name),(void**) ret_serv);
    if(!ret_serv){
        return LIB_ERR_NAMESERVICE_UNKNOWN_NAME;
    }else{
        return SYS_ERR_OK;
    }
}

bool verify_name(const char* name){
    return true;
}

void remove_server(struct server_list* del_server){
    if(servers == del_server){
        servers = del_server -> next;
        free(del_server);   
        return;
    }
    struct server_list* curr = servers;
    for(;curr -> next  != NULL;curr = curr -> next){
        if(curr -> next == del_server){
            curr -> next = del_server->next;
        }
    }
    n_servers--;

    server_ht -> d.remove(&server_ht -> d, del_server -> name, strlen(del_server -> name));
    free(del_server);

}

void print_server_list(void){
    debug_printf("================ Servers ==============================\n");
    for(struct server_list * curr = servers; curr != NULL; curr = curr -> next){
        debug_printf("|| P: %d | C: %d | N: %s | Direct: %d |               \n", curr -> pid, curr -> core_id, curr -> name,curr -> direct);

        for(int i =0 ;i < 64;++i){
            if(curr -> key[i] != NULL && curr -> value[i] != NULL){
                debug_printf("%s=%s\n",curr -> key[i],curr ->value[i]);
            }
        }
        // debug_printf("-> %s\n",curr -> properties);
        //TODO print properties
    }


    debug_printf("=======================================================\n");
}


void find_servers_by_prefix(const char* name, char* response,size_t * resp_size){
    *resp_size = 0;
    *response = '\0';
    for(struct server_list* curr = servers;curr != NULL;curr = curr -> next){
        if(prefix_match((char*) name,(char*) curr -> name)){            
            if(*resp_size > 0){
                strcat(response,",");
            }
            (*resp_size) += 1;
            strcat(response,curr -> name);
        }
}
}


bool prefix_match(char* name, char* server_name){
    if(*name == '/'){
        return true;
    }
    while(*name != '\0'){
        if(*server_name == '\0'){return false;}
        if(*name++ != *server_name++){
            return false;
        }
    }
    return true;
}