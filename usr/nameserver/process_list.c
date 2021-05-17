#include "process_list.h"

#include <aos/aos.h>
#include <aos/aos_rpc.h>



errval_t add_process(coreid_t core_id,const char* name,domainid_t pid,struct aos_rpc* rpc ){
    

    struct process * p = (struct process * ) malloc(sizeof(struct process));
    // *pid = process++;
    p -> next = NULL,
    p -> pid = pid;
    p -> core_id = core_id;
    p -> name = strcopy(name);
    p -> rpc  = rpc;

    if(pl.head == NULL){
        pl.head = p;
        pl.tail = p;
    }else{
        pl.tail -> next = p;
        pl.tail = p;
    }
    pl.size++;

    print_process_list();
    return SYS_ERR_OK;
};


errval_t get_core_id(domainid_t pid, coreid_t *core_id){
    struct process  * curr = pl.head;
    for(;curr != NULL; curr = curr -> next){
        if(curr -> pid == pid){
            *core_id = curr -> core_id;
            return SYS_ERR_OK;
        }
    }
    return PROC_MGMT_ERR_DOMAIN_NOT_RUNNING;
}


errval_t find_process_by_rpc(struct aos_rpc *rpc,domainid_t * res_pid){
    struct process  * curr = pl.head;
    for(;curr != NULL; curr = curr -> next){
        if(curr -> rpc == rpc){
            *res_pid = curr -> pid;
            return SYS_ERR_OK;
        }
    }
    *res_pid = -1;
    return PROC_MGMT_ERR_DOMAIN_NOT_RUNNING;
}

char* strcopy(const char* str){
    size_t n = strlen(str) + 1;
    char * new_str = (char * ) malloc( n * sizeof(char));
    strncpy(new_str, str, n);
    return new_str;
}

void print_process_list(void){
    debug_printf("================ Processes ============================\n");
    for(struct process* curr = pl.head; curr != NULL; curr = curr -> next){
        debug_printf("|| Pid:   %d  Core:   %d  Name:  %s         \n",curr -> pid, curr -> core_id, curr -> name);
    }
    debug_printf("========================================================\n");
}

errval_t remove_process_by_pid(struct aos_rpc* rpc, domainid_t pid){

    struct process * temp;
    if(pl.head -> pid == pid && rpc == pl.head -> rpc){
        temp = pl.head;
        pl.head = pl.head -> next;
        free(temp);
        return SYS_ERR_OK;
    }
    struct process * curr;
    for(curr = pl.head; curr != pl.tail;curr = curr -> next){
        if(curr->next->pid == pid  && rpc == curr -> next -> rpc){
            temp = curr -> next;
            curr -> next = curr -> next -> next;
            free(temp);
            return SYS_ERR_OK;
        }
    }
    return LIB_ERR_PROC_MNGMT_INVALID_DEREG;
   
}