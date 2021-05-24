#include "routing.h"
#include <hashtable/hashtable.h>


// static struct routing_entry* routing_head;

// struct routing_ht


errval_t add_routing_entry(struct routing_entry * re){
    // errval_t err;
    //  server_ht -> d.get(&server_ht ->d,name,strlen(name),(void**) ret_serv);
    // if(routing_head == NULL){
    //     routing_head = re;
    // }else{
    //     struct routing_entry * curr = routing_head;
    //     for(;curr -> next != NULL;curr = curr -> next){}
    //     curr -> next  = re;
    // }

    int failed = routing_ht ->d.put_word(&routing_ht ->d,re -> name,strlen(re -> name),(uintptr_t) re);
    if(failed){
        return LIB_ERR_NAMESERVICE_HASHTABLE_ERROR;
    }
    return SYS_ERR_OK;
}

 errval_t get_routing_entry_by_name(const char* name,struct routing_entry** ret_re){
    // struct routing_entry * curr = routing_head;
    // for(;curr != NULL;curr = curr -> next){
    //     if(!strcmp(curr ->  name,name)){
    //         return curr;
    //     }
    // }
    // return NULL;
    // errval_t err;

    routing_ht ->d.get(&routing_ht ->d,name,strlen(name),(void**) ret_re);
    if(!ret_re){
        return LIB_ERR_NAMESERVICE_ROUTING_ERROR;
    }else{
        return SYS_ERR_OK;
    }
}

errval_t remove_routing_entry(const char * name){
    routing_ht ->d.remove(&routing_ht -> d,name,strlen(name));
    return SYS_ERR_OK;
}