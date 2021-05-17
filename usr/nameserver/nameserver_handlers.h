#ifndef NAME_SERVER_HANDLERS_H
#define NAME_SERVER_HANDLERS_H
#include <stdio.h>
#include <stdlib.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>



void initialize_ns_handlers(struct aos_rpc * init_rpc);
void initialize_nameservice_handlers(struct aos_rpc *ns_rpc);

void handle_reg_proc(struct aos_rpc *rpc,uintptr_t core_id,const char* name,struct capref proc_ep_cap, uintptr_t pid, struct capref* ns_ep_cap);
void handle_reg_init(struct aos_rpc *rpc,uintptr_t core_id,const char* name, uintptr_t * pid);
void handle_get_proc_name(struct aos_rpc *rpc, uintptr_t pid,char* name);
void handle_get_proc_core(struct aos_rpc* rpc, uintptr_t pid,uintptr_t *core);
void handle_get_proc_list(struct aos_rpc *rpc, uintptr_t *size,char * pids);
void handle_pid_request(struct aos_rpc *rpc,uintptr_t* pid);
void handle_server_request(struct aos_rpc * rpc, uintptr_t pid, uintptr_t core_id ,const char* server_data, struct capref server_ep_cap, const char * return_message);
void handle_server_lookup(struct aos_rpc *rpc, char *name,uintptr_t* core_id,uintptr_t *ump,struct capref* server_ep_cap);
#endif 