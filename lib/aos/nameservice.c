/**
 * \file nameservice.h
 * \brief 
 */
#include <stdio.h>
#include <stdlib.h>

#include <aos/aos.h>
#include <aos/waitset.h>
#include <aos/nameserver.h>
#include <aos/aos_rpc.h>
#include <stdarg.h>
#include <aos/default_interfaces.h>


#include <hashtable/hashtable.h>

static struct srv_entry* head;
static void *get_opaque_server_rpc_handlers[OS_IFACE_N_FUNCTIONS];
// * server_table = create_hashtable();




struct srv_entry {
	struct srv_entry * next;
	const char *name;
	nameservice_receive_handler_t *recv_handler;
	void *st;
};

struct nameservice_chan 
{
	struct aos_rpc rpc;
	char *name;
};




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
                         struct capref tx_cap, struct capref rx_cap)
{
	return LIB_ERR_NOT_IMPLEMENTED;
}




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
	                              void *st)
{	
	
	return nameservice_register_properties(name,recv_handler,st,true);
}


errval_t nameservice_register_properties(const char * name,nameservice_receive_handler_t recv_handler, void * st, bool ump,...){

	va_list args;
	va_start(args, ump);
	errval_t err;

	// create and add srv_entry
	struct srv_entry* new_srv_entry = (struct srv_entry *) malloc(sizeof(struct srv_entry));
	new_srv_entry -> next = NULL;
	new_srv_entry -> name = name;
	new_srv_entry -> recv_handler = recv_handler;
	new_srv_entry -> st = st;
	if(head == NULL){
		head = new_srv_entry;
	}else {
		struct srv_entry * curr = head;
		for(; curr -> next != NULL;curr = curr -> next){}
		curr -> next = new_srv_entry;
	}

	struct capref server_ep;
	if(ump){
		err = create_ump_server_ep(&server_ep);
	}else{
		err = create_lmp_server_ep(&server_ep);
	}
	ON_ERR_RETURN(err);

	// char buffer[512];
	// struct capability cap;
	// invoke_cap_identify(server_ep,&cap);
	// debug_print_cap(buffer,512,&cap);
	// debug_printf("Cap: %s\n",buffer);
	char buf[512];
	err = aos_rpc_call(get_init_rpc(),INIT_REG_SERVER,disp_get_domain_id(),name,server_ep,"hi",buf);
	ON_ERR_RETURN(err);
	//TODO: rich properties
	//TODO: lmp ep


	return SYS_ERR_OK;
}

errval_t create_ump_server_ep(struct capref* server_ep){
	errval_t err;
	struct aos_rpc* new_rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
	err = slot_alloc(server_ep);
	size_t urpc_cap_size;
	err  = frame_alloc(server_ep,BASE_PAGE_SIZE,&urpc_cap_size);
	ON_ERR_RETURN(err);
	ON_ERR_RETURN(err);
	char *urpc_data = NULL;
	err = paging_map_frame_complete(get_current_paging_state(), (void **) &urpc_data, *server_ep, NULL, NULL);
	err =  aos_rpc_init_ump_default(new_rpc,(lvaddr_t) urpc_data, BASE_PAGE_SIZE,true);
	ON_ERR_RETURN(err);
	err = aos_rpc_set_interface(new_rpc,get_opaque_server_interface(),OS_IFACE_N_FUNCTIONS,get_opaque_server_rpc_handlers);
	ON_ERR_RETURN(err);
	return SYS_ERR_OK;

}

errval_t create_lmp_server_ep(struct capref* server_ep){
	errval_t err;
	struct aos_rpc* new_rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
	new_rpc -> lmp_server_mode  = true; // NOTE: unsure?
	err = slot_alloc(server_ep);
	ON_ERR_RETURN(err);
	struct lmp_endpoint * lmp_ep;
	err = endpoint_create(256,server_ep,&lmp_ep);
	ON_ERR_RETURN(err);
	err = aos_rpc_init_lmp(new_rpc,*server_ep,NULL_CAP,lmp_ep, get_default_waitset());
	ON_ERR_RETURN(err);
	return SYS_ERR_OK;
}


/**
 * @brief deregisters the service 'name'
 *
 * @param the name to deregister
 * 
 * @return error value
 */
errval_t nameservice_deregister(const char *name)
{
	return LIB_ERR_NOT_IMPLEMENTED;
}


/**
 * @brief lookup an endpoint and obtain an RPC channel to that
 *
 * @param name  name to lookup
 * @param chan  pointer to the chan representation to send messages to the service
 *
 * @return  SYS_ERR_OK on success, errval on failure
 */
errval_t nameservice_lookup(const char *name, nameservice_chan_t *nschan)
{
	return LIB_ERR_NOT_IMPLEMENTED;
}


/**
 * @brief enumerates all entries that match an query (prefix match)
 * 
 * @param query     the query
 * @param num 		number of entries in the result array
 * @param result	an array of entries
 */
errval_t nameservice_enumerate(char *query, size_t *num, char **result )
{
	return LIB_ERR_NOT_IMPLEMENTED;
}



void nameservice_reveice_handler_wrapper(struct aos_rpc * rpc,struct aos_rpc_varbytes message,struct capref tx_cap, struct aos_rpc_varbytes * response, struct capref* rx_cap){

	

	// ht_get_capability()
	//find correct namerver (map channel -> name)

}