/**
 * \file nameservice.h
 * \brief 
 */

#ifndef INCLUDE_NAMESERVICE_H_
#define INCLUDE_NAMESERVICE_H_

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <stdarg.h>



#define PROPERTY_MAX_SIZE 512
#define SERVER_NAME_SIZE 128





typedef void* nameservice_chan_t;


struct server_connection {
	const char* name;
	coreid_t core_id;
	bool ump;
	struct aos_rpc * rpc;
	bool dead;
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


errval_t nameservice_register_properties(const char * name,nameservice_receive_handler_t recv_handler, void * st, bool ump,const char * properties);

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
 * @brief enumerates all entries that match an query (prefix match)
 * 
 * @param query     the query
 * @param num 		number of entries in the result array
 * @param result	an array of entries
 */
errval_t nameservice_enumerate(char *query, size_t *num, char **result );



void nameservice_reveice_handler_wrapper(struct aos_rpc * rpc,char*  message,struct capref tx_cap, char * response, struct capref* rx_cap);


errval_t create_ump_server_ep(struct capref* server_ep,struct aos_rpc** ret_rpc);
errval_t create_lmp_server_ep(struct capref* server_ep, struct aos_rpc** ret_rpc);
errval_t serialize(const char * name, const char * properties,char** ret_server_data);
errval_t deserialize_prop(const char * server_data,char *  key[],char *  value[], char**name);
errval_t get_properties_size(char * properties,size_t * size);
errval_t establish_init_server_con(const char* name,struct aos_rpc* server_rpc, struct capref local_cap);
void init_server_handlers(struct aos_rpc* server_rpc);

bool name_check(const char*name);
bool property_check(const char * properties);
bool query_check(const char*query);
#endif /* INCLUDE_AOS_AOS_NAMESERVICE_H_ */

