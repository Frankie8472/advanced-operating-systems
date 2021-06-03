#include "server_list.h"
#include "process_list.h"
#include <hashtable/hashtable.h>


#include <aos/nameserver.h>






errval_t add_server(struct server_list* new_server){
    struct server_list* curr = servers;
    if(curr == NULL){
        servers = new_server;
    }
    else {
        
        for(;curr -> next != NULL;curr = curr -> next){
            // debug_printf("%s =? %s\n",new_server->name, curr -> name);
            if(!strcmp(new_server-> name, curr -> name)){
                free_server(new_server);
                return LIB_ERR_NAMESERVICE_INVALID_REGISTER;
            }
        }

        if(!strcmp(new_server-> name, curr -> name)){
            free_server(new_server);
            return LIB_ERR_NAMESERVICE_INVALID_REGISTER;
        }
        curr -> next = new_server;
    }
    n_servers++;

    // print_server_list();
    int failed = server_ht -> d.put_word(&server_ht ->d,new_server -> name,strlen(new_server -> name),(uintptr_t) new_server);
    if(failed){
        return LIB_ERR_NAMESERVICE_HASHTABLE_ERROR;
    }
    return SYS_ERR_OK;
}

errval_t find_server_by_name(char * name, struct server_list ** ret_serv){

    server_ht -> d.get(&server_ht ->d,name,strlen(name),(void**) ret_serv);
    printf("ret_server: %lx\n",*ret_serv);
    if(!*ret_serv){
        return LIB_ERR_NAMESERVICE_UNKNOWN_NAME;
    }else{
        return SYS_ERR_OK;
    }
}



void remove_server(struct server_list* del_server){
    if(servers == del_server){
        servers = del_server -> next;
        free_server(del_server); 
        server_ht -> d.remove(&server_ht -> d, del_server -> name, strlen(del_server -> name));  
        return;
    }
    struct server_list* curr = servers;
    for(;curr -> next  != NULL;curr = curr -> next){
        if(curr -> next == del_server){
            curr -> next = del_server->next;
            break;
        }
    }
    n_servers--;

    server_ht -> d.remove(&server_ht -> d, del_server -> name, strlen(del_server -> name));
    free_server(del_server);

}

void print_server_list(void){
    debug_printf("================ Servers ==============================\n");
    for(struct server_list * curr = servers; curr != NULL; curr = curr -> next){
        debug_printf("|| P: %d | C: %d | N: %s | Direct: %d | Prop_size: %d              \n", curr -> pid, curr -> core_id, curr -> name,curr -> direct,curr -> n_properties);

        for(int i =0 ;i < 64;++i){
            if(curr -> key[i] != NULL && curr -> value[i] != NULL){
                debug_printf("%s=%s\n",curr -> key[i],curr ->value[i]);
            }
        }
    }


    debug_printf("=======================================================\n");
}


errval_t find_server_by_name_and_property(const char * name, char*  keys[],char*  values[],size_t prop_size,struct server_list ** ret_serv){
    struct server_list* curr = servers;
    while(curr != NULL){
        if(prefix_match((char*)name,curr -> name) && property_match(curr,keys,values,prop_size)){
            *ret_serv = curr;
            return SYS_ERR_OK;
        }
        curr = curr -> next;
    }
    return LIB_ERR_NAMESERVICE_UNKNOWN_NAME;
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

void find_servers_by_prefix_and_prop(const char* name,char*  keys[],char*  values[],size_t prop_size , char* response,size_t * resp_size){
    *resp_size = 0;
    *response = '\0';
    for(struct server_list* curr = servers;curr != NULL;curr = curr -> next){
        if(prefix_match((char*) name,(char*) curr -> name) && property_match(curr,keys,values,prop_size)){            
            if(*resp_size > 0){
                strcat(response,",");
            }
            (*resp_size) += 1;
            strcat(response,curr -> name);
        }
    }
}


bool prefix_match(char* name, char* server_name){
    while(*name != '\0'){
        if(*server_name == '\0'){return false;}
        if(*name++ != *server_name++){
            return false;
        }
    }
    return true;
}

void free_server(struct server_list* server){
    for(size_t i = 0; i < N_PROPERTIES;++i){
        if(server -> key[i]){
            free(server -> key[i]);
        }
        if(server -> value[i]){
            free(server -> value[i]);
        }
    }
    free(server);
}

bool property_match(struct server_list* server, char *  keys[],char* values[], size_t prop_size){
    for(size_t j = 0; j < prop_size;++j){
        char* q_key = keys[j];
        char* q_value = values[j];
        bool match = false;
        for(size_t i = 0;i < server->n_properties;++i){
            char* key = server -> key[i];
            char* value = server -> value[i];
            if(!strcmp(q_key,key) && !strcmp(q_value,value)){
                match = true;
                break;
            }
        }
        if(match == false){return false;}
        
    }
    return true;
}
