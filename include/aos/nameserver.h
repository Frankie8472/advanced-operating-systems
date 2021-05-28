/**
 * \file nameservice.h
 * \brief 
 */

#ifndef INCLUDE_NAMESERVICE_H_
#define INCLUDE_NAMESERVICE_H_

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <stdarg.h>



#define PROPERTY_MAX_SIZE 128
#define SERVER_NAME_SIZE 128
#define N_PROPERTIES 64
#define NS_SWEEP_INTERVAL 10000000
#define NS_LIVENESS_INTERVAL 1000000


typedef void* nameservice_chan_t;


struct server_connection {
	const char* name;
	coreid_t core_id;
	bool direct;
	struct aos_rpc * rpc;
};


///< handler which is called when a message is received over the registered channel
typedef void(nameservice_receive_handler_t)(void *st, 
										    void *message, size_t bytes,
										    void **response, size_t *response_bytes,
                                            struct capref tx_cap, struct capref *rx_cap);





/**
 * @brief sends a message back to the client who sent us a message
 *
 * @param chan opaque handle of the channel
 * @oaram message pointer to the message
 * @param bytes size of the message in bytes
 * @param response the response message
 * @param response_byts the size of the response
 * 
 * @return error value
 */
errval_t nameservice_rpc(nameservice_chan_t chan, void *message, size_t bytes, 
                         void **response, size_t *response_bytes,
                         struct capref tx_cap, struct capref rx_cap);



/**
 * @brief registers our selves as 'name'
 *
 * @param name  our name
 * @param recv_handler the message handler for messages received over this service
 * @param st  state passed to the receive handler
 *
 * @return SYS_ERR_OK
 */
errval_t nameservice_register(const char *name, 
	                              nameservice_receive_handler_t recv_handler,
	                              void *st);


errval_t nameservice_register_direct(const char *name, 
	                              nameservice_receive_handler_t recv_handler,
	                              void *st);
errval_t nameservice_register_properties(const char * name,nameservice_receive_handler_t recv_handler, void * st, bool direct,const char * properties);

/**
 * @brief deregisters the service 'name'
 *
 * @param the name to deregister
 * 
 * @return error value
 */
errval_t nameservice_deregister(const char *name);


/**
 * @brief lookup an endpoint and obtain an RPC channel to that
 *
 * @param name  name to lookup
 * @param chan  pointer to the chan representation to send messages to the service
 *
 * @return  SYS_ERR_OK on success, errval on failure
 */
errval_t nameservice_lookup(const char *name, nameservice_chan_t *chan);



/**
 * @brief creates a channel to the a server with the same prefix as name and has 	all the properites in properties
 *
 * @param name  name to lookup

 * @param properties propert

 * @param chan  pointer to the chan representation to send messages to the service
 *
 * @return  SYS_ERR_OK on success, errval on failure
 */


errval_t nameservice_lookup_with_prop(const char *name,char * properties, nameservice_chan_t *nschan);

/**
 * @brief enumerates all entries that match an query (prefix match)
 * 
 * @param query     the query
 * @param num 		number of entries in the result array
 * @param result	an array of entries
 */
errval_t nameservice_enumerate(char *query, size_t *num, char **result );


/**
 * @brief get properties of a server, caller is responseible for free pointer
 * 
 * @param name     servername
 * @param reponse	pointer to pointer, which will be filled in with single 					string of properties
 */
errval_t nameservice_get_props(const char* name, char ** response);

void nameservice_reveice_handler_wrapper(struct aos_rpc * rpc,char*  message,struct capref tx_cap, char * response, struct capref* rx_cap);
void namservice_receive_handler_wrapper_direct(struct aos_rpc *rpc, struct aos_rpc_varbytes  message,struct aos_rpc_varbytes * response);
void nameservice_binding_request_handler(struct aos_rpc *rpc,uintptr_t remote_core_id, struct capref remote_cap, struct capref* local_cap);
errval_t nameservice_create_nshan(const char *name,bool direct , coreid_t core_id, nameservice_chan_t * nschan);

errval_t create_ump_server_ep(struct capref* server_ep,struct aos_rpc** ret_rpc,bool first_half);
errval_t create_lmp_server_ep(struct capref* server_ep, struct aos_rpc** ret_rpc);
errval_t serialize(const char * name, const char * properties,char** ret_server_data);
errval_t deserialize_prop(const char * server_data,char *  key[],char *  value[], char**name,size_t * property_size);
errval_t get_properties_size(char * properties,size_t * size);
errval_t establish_init_server_con(const char* name,struct aos_rpc* server_rpc, struct capref local_cap);
errval_t nameservice_lookup_query(const char * name,const char * query, nameservice_chan_t *nschan);
void init_server_handlers(struct aos_rpc* server_rpc);
errval_t create_lmp_server_ep_with_struct_aos_rpc(struct capref* server_ep, struct aos_rpc* new_rpc);
bool name_check(const char*name);
bool property_check(const char * properties);
bool query_check(const char*query);
#endif /* INCLUDE_AOS_AOS_NAMESERVICE_H_ */

