#include "process_list.h"

#include <aos/aos.h>
#include <aos/aos_rpc.h>



errval_t add_process(coreid_t core_id,const char* name,domainid_t *pid,struct aos_rpc* rpc ){
    

    struct process * p = (struct process * ) malloc(sizeof(struct process));
    *pid = process++;
    p -> next = NULL,
    p -> pid = *pid;
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

char* strcopy(const char* str){
    size_t n = strlen(str) + 1;
    char * new_str = (char * ) malloc( n * sizeof(char));
    strncpy(new_str, str, n);
    return new_str;
}

void print_process_list(void){
    debug_printf("================ Processes ====================\n");
    for(struct process* curr = pl.head; curr != NULL; curr = curr -> next){
        debug_printf("Pid:   %d  Core:   %d  Name:   %s\n",curr -> pid, curr -> core_id, curr -> name);
    }
}