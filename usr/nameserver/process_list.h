#ifndef PROCESS_LIST_H
#define PROCESS_LIST_H
#include <stdio.h>
#include <stdlib.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>


struct process_list pl;
domainid_t process;

struct process {
    struct process * next;
    domainid_t pid;
    coreid_t core_id;
    char* name;
    struct aos_rpc* rpc;
};


struct process_list{
    struct process *head;
    struct process *tail;
    size_t size;
};


char* strcopy(const char* str);

errval_t add_process(coreid_t core_id,const char* name,domainid_t pid,struct aos_rpc* rpc );
errval_t find_process_by_rpc(struct aos_rpc *rpc,domainid_t * res_pid);
errval_t get_core_id(domainid_t pid, coreid_t *core_id);
void print_process_list(void);

#endif 