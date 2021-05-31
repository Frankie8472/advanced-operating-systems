#ifndef NAME_SERVER_HANDLERS_H
#define NAME_SERVER_HANDLERS_H
#include <stdio.h>
#include <stdlib.h>
#include <aos/aos.h>
#include <aos/aos_rpc.h>


/**
 * @brief Init handlers for init channel and ns channel
 */
void initialize_ns_handlers(struct aos_rpc * init_rpc);
void initialize_nameservice_handlers(struct aos_rpc *ns_rpc);
/**
 * @brief Handle registration and deregistration
 */
void handle_reg_server(struct aos_rpc * rpc, uintptr_t pid, uintptr_t core_id ,const char* server_data, uintptr_t direct, char * return_message);
void handle_dereg_server(struct aos_rpc *rpc, const char* name, uintptr_t* success);


/**
 * @brief Handle lookup
 */
void handle_server_lookup(struct aos_rpc *rpc, char *name,uintptr_t* core_id,uintptr_t *direct,uintptr_t * success);
void handle_server_lookup_with_prop(struct aos_rpc *rpc, char *query,uintptr_t* core_id,uintptr_t *direct,uintptr_t * success, char * response_name);

/**
 * @brief enumeration of server
 */
void handle_enum_servers(struct aos_rpc *rpc,const char* name, char * response, uintptr_t * resp_size);
void handle_server_enum_with_prop(struct aos_rpc * rpc, char* query,uintptr_t* num,char * response);

/**
 * @brief handle get server stats
 */
void handle_get_props(struct aos_rpc *rpc,const char* name, char * response);
void handle_get_server_pid(struct aos_rpc *rpc, const char * name, uintptr_t* pid );

/**
 * @brief handle liveness checks
 */
void handle_liveness_check(struct aos_rpc *rpc, const char* name);
#endif 