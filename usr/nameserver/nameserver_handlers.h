#ifndef NAME_SERVER_HANDLERS_H
#define NAME_SERVER_HANDLERS_H
#include <stdio.h>
#include <stdlib.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>



void initialize_ns_handlers(struct aos_rpc * init_rpc);
void initialize_nameservice_handlers(struct aos_rpc *ns_rpc);

void handle_server_request(struct aos_rpc * rpc, uintptr_t pid, uintptr_t core_id ,const char* server_data, struct capref server_ep_cap, const char * return_message);
void handle_server_lookup(struct aos_rpc *rpc, char *name,uintptr_t* core_id,uintptr_t *ump,struct capref* server_ep_cap);
void handle_dereg_server(struct aos_rpc *rpc, const char* name, uintptr_t* success);
#endif 