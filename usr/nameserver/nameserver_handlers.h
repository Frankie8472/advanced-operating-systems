#ifndef NAME_SERVER_HANDLERS_H
#define NAME_SERVER_HANDLERS_H
#include <stdio.h>
#include <stdlib.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>



void initialize_ns_handlers(struct aos_rpc * init_rpc);
void handle_reg_proc(struct aos_rpc *rpc,uintptr_t core_id,const char* name,struct capref proc_ep_cap, uintptr_t * pid, struct capref* ns_ep_cap);
void handle_reg_init(struct aos_rpc *rpc,uintptr_t core_id,const char* name, uintptr_t * pid);

#endif 