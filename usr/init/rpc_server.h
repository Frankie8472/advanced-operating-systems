#ifndef INIT_RPC_SERVER_H_
#define INIT_RPC_SERVER_H_


#include <errno.h>
#include <aos/aos_rpc.h>

errval_t init_terminal_state(void);


extern struct aos_rpc* core_channels[4];

enum SERVICES {
    PROCESS_MANAGER,
    MEMORY_SERVER
} ;

errval_t init_core_channel(coreid_t coreid, lvaddr_t urpc_frame);
void register_core_channel_handlers(struct aos_rpc *rpc);

errval_t initialize_rpc_handlers(struct aos_rpc *rpc);


void handle_putchar(struct aos_rpc *r, uintptr_t c);
void handle_getchar(struct aos_rpc *r, uintptr_t *c);
void handle_request_ram(struct aos_rpc *r, uintptr_t size,
                        uintptr_t alignment, struct capref *cap,
                        uintptr_t *ret_size);
void handle_initiate(struct aos_rpc *rpc, struct capref cap);
void handle_spawn(struct aos_rpc *old_rpc, const char *name,
                  uintptr_t core_id, uintptr_t *new_pid);
void handle_foreign_spawn(struct aos_rpc *origin_rpc, const char *name,
                          uintptr_t core_id, uintptr_t *new_pid);
void handle_send_number(struct aos_rpc *r, uintptr_t number);
void handle_send_string(struct aos_rpc *r, const char *string);
void handle_service_on(struct aos_rpc *r, uintptr_t service);
void handle_init_process_register(struct aos_rpc *r,uintptr_t pid,uintptr_t core_id,const char* name);
void handle_mem_server_request(struct aos_rpc *r, struct capref client_cap, struct capref * server_cap);


#endif // INIT_RPC_SERVER_H_
