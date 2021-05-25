#ifndef PROC_MAN_HANDLERS_H_
#define PROC_MAN_HANDLERS_H_

#include <stdio.h>
#include <stdlib.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>


void handle_pid_request(struct aos_rpc *rpc,uintptr_t* pid);
void handle_reg_proc(struct aos_rpc *rpc,uintptr_t core_id,const char* name,struct capref proc_ep_cap, uintptr_t pid, struct capref* ns_ep_cap);
void handle_reg_init(struct aos_rpc *rpc,uintptr_t core_id,const char* name, uintptr_t * pid);
void handle_get_proc_name(struct aos_rpc *rpc, uintptr_t pid,char* name);
void handle_get_proc_core(struct aos_rpc* rpc, uintptr_t pid,uintptr_t *core);
void handle_get_proc_list(struct aos_rpc *rpc, uintptr_t *size,char * pids);
void handle_dereg_process(struct aos_rpc * rpc, uintptr_t pid);

#endif 