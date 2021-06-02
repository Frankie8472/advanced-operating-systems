/**
 * \file nameservice.h
 * \brief 
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <regex.h>
#include <aos/aos.h>
#include <aos/deferred.h>
#include <aos/waitset.h>
#include <aos/nameserver.h>
#include <aos/systime.h>
#include <aos/aos_rpc.h>
#include <stdarg.h>
#include <aos/default_interfaces.h>

#include <hashtable/hashtable.h>


#include <sys/cdefs.h>


static void *get_opaque_server_rpc_handlers[OS_IFACE_N_FUNCTIONS];
// * server_table = create_hashtable();

struct srv_entry {
	struct srv_entry* next;
	const char *name;
	nameservice_receive_handler_t *recv_handler;
	void *st;
	struct periodic_event liveness_checker;
	struct aos_rpc main_handler;
};


static struct srv_entry* server_entries;

static void add_server(struct srv_entry* srv_entry){
	struct srv_entry* curr = server_entries; 
	if(curr == NULL){
		server_entries = srv_entry;
	}
	else{
		for(;curr -> next != NULL; curr = curr -> next){}
		curr -> next = srv_entry;
	}
}

static void remove_server(const char *name){
	struct srv_entry* curr = server_entries;  
	errval_t err;
	if(!strcmp(curr -> name, name)){
		server_entries = curr -> next;
		err = periodic_event_cancel(&curr -> liveness_checker);
		if(err_is_fail(err)){
			DEBUG_ERR(err,"Failed to cancel liveness checker!\n");
		}
		free(curr);
	}
	else{
		for(;curr -> next != NULL;curr = curr -> next){
			if((!strcmp(curr -> next -> name, name))){
				struct srv_entry* temp = curr -> next;
				curr -> next = temp -> next;
				err = periodic_event_cancel(&temp -> liveness_checker);
				if(err_is_fail(err)){
					DEBUG_ERR(err,"Failed to cancel liveness checker!\n");
				}
				
				free(temp);
			}
		}
	}
}


static void liveness_checker(void* name){
	// debug_printf("Liveness checker: %s\n",name);
	errval_t err = aos_rpc_call(get_ns_rpc(),NS_LIVENESS_CHECK,(const char*) name);
	if(err_is_fail(err)){
		DEBUG_ERR(err,"Failed to send liveness check");
	}
}




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

	// uint64_t count = 0;
    // uint64_t sum = 0;


	errval_t err;
	assert(chan && "Invalid namservice channel!");
	// uint64_t start = systime_to_ns(systime_now());
	struct server_connection *serv_con = (struct server_connection *) chan;
	char * response_buffer = (void * ) malloc(MAX_SERVER_MESSAGE_SIZE);
	struct aos_rpc_varbytes resp_varbytes = {
		.bytes = response_buffer,
		.length = MAX_SERVER_MESSAGE_SIZE
	};
	struct aos_rpc_varbytes msg_varbytes;
	msg_varbytes.length = bytes;
	msg_varbytes.bytes = (char* ) message;
	size_t response_size;


	// uint64_t end = systime_to_ns(systime_now());
	// uint64_t tts = end - start;
	// debug_printf("%lu\n",tts);

	if(serv_con -> direct){
		if(!capref_is_null(tx_cap) && !capref_is_null(rx_cap)){
			debug_printf("Trying to send or recv cap over a direct server connection!(impossible to do!)\n");
			return LIB_ERR_NOT_IMPLEMENTED;
		}
		// uint64_t start = systime_to_ns(systime_now());
		err = aos_rpc_call(serv_con -> rpc,OS_IFACE_DIRECT_MESSAGE,msg_varbytes,&resp_varbytes,&response_size);
		// uint64_t end = systime_to_ns(systime_now());
		// uint64_t tts = end - start;
		// debug_printf("%lu\n",tts);
		ON_ERR_RETURN(err);


	}else{
		
		struct capref response_cap;
		slot_alloc(&response_cap);

		if(capref_is_null(rx_cap) && capref_is_null(tx_cap)){ //no ret no senc cap
			err = aos_rpc_call(serv_con -> rpc,INIT_CLIENT_CALL2,serv_con -> core_id,serv_con -> name,msg_varbytes,&resp_varbytes,&response_size);
		}else if(capref_is_null(rx_cap)){ // no ret cap
			err = aos_rpc_call(serv_con -> rpc,INIT_CLIENT_CALL1,serv_con -> core_id,serv_con -> name,msg_varbytes,tx_cap,&resp_varbytes,&response_size);
		}
		else if(capref_is_null(tx_cap)){ //no send cap
			err = aos_rpc_call(serv_con -> rpc,INIT_CLIENT_CALL3,serv_con -> core_id,serv_con -> name,msg_varbytes,&resp_varbytes,&response_cap,&response_size);
		}else{
			err = aos_rpc_call(serv_con -> rpc,INIT_CLIENT_CALL,serv_con -> core_id,serv_con -> name,msg_varbytes,tx_cap,&resp_varbytes,&response_cap,&response_size);
		}
		ON_ERR_RETURN(err);
		cap_copy(rx_cap,response_cap);
		
		// *response = realloc(response_buffer,response_size);

	}
	*response = response_buffer;
	*response_bytes = response_size;
	resp_varbytes.length = response_size;
	return SYS_ERR_OK;
}




/**
 * @brief registers our selves as 'name', indirect server
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
	
	return nameservice_register_properties(name,recv_handler,st,false,"type=default");
}
/**
 * @brief registers our selves as 'name' and a direct server
 *
 * @param name  our name
 * @param recv_handler the message handler for messages received over this service
 * @param st  state passed to the receive handler
 *
 * @return SYS_ERR_OK
 */

errval_t nameservice_register_direct(const char *name, 
	                              nameservice_receive_handler_t recv_handler,
	                              void *st)
{	
	
	return nameservice_register_properties(name,recv_handler,st,true,"type=default");
}


/**
 * @brief registers our selves as name with properties 'properties'
 *
 * @param name  our name
 * @param recv_handler the message handler for messages received over this service
 * @param st  state passed to the receive handler
 * @param direct  register server as direct
 * @param properties  properites of server 
 *
 * @return SYS_ERR_OK
 */


errval_t nameservice_register_properties(const char * name,nameservice_receive_handler_t recv_handler, void * st, bool direct,const char * properties){
	errval_t err;


	if(!name_check(name)){
		return LIB_ERR_NAMESERVICE_INV_SERVER_NAME;
	}

	if(!property_check(properties)){
		return LIB_ERR_NAMESERVICE_INVALID_PROPERTY_FORMAT;
	}

	char* server_data;
	err = serialize(name,properties,&server_data);
	ON_ERR_RETURN(err);
	uint64_t success;
	err = aos_rpc_call(get_ns_rpc(),NS_REG_SERVER,disp_get_domain_id(),disp_get_core_id(),server_data,direct,&success);
	if(err_is_fail(err)){
		
		
		return err;
	}
	if(!success){
		printf("Server name: %s already in use!\n",name);
		return LIB_ERR_NAMESERVICE_INVALID_REGISTER;
	}
	struct srv_entry* new_srv_entry = (struct srv_entry *) malloc(sizeof(struct srv_entry));
	new_srv_entry -> name = name;
	new_srv_entry -> recv_handler = recv_handler;
	new_srv_entry -> st = st;
	
	err = periodic_event_create(&new_srv_entry -> liveness_checker,get_default_waitset(),NS_LIVENESS_INTERVAL,MKCLOSURE(liveness_checker,(void*) name));
	ON_ERR_RETURN(err);
	struct capref server_ep;
	err = create_lmp_server_ep_with_struct_aos_rpc(&server_ep,&new_srv_entry -> main_handler);

	ON_ERR_RETURN(err);


	err = establish_init_server_con(name,&new_srv_entry -> main_handler,server_ep);
	ON_ERR_RETURN(err);
	if(err_is_fail(err)){
		nameservice_deregister(name);
		free(new_srv_entry);
		DEBUG_ERR(err,"Failed to init server connection with rpc server\n");
	}

	new_srv_entry -> main_handler.serv_entry = (void*) new_srv_entry;
	add_server(new_srv_entry);
    return SYS_ERR_OK;
}


/**
 * @brief create ump server endpoint 
 *
 * @param server_ep  our endpoint cap ( to be filled in )
 * @param ret_rpc struct aos_rpc to be created and returned
 * @param first_half  take first half of UMP frame

 *
 * @return SYS_ERR_OK
 */


errval_t create_ump_server_ep(struct capref* server_ep,struct aos_rpc** ret_rpc,bool first_half){
	errval_t err;
	struct aos_rpc* new_rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
	err = slot_alloc(server_ep);
	ON_ERR_RETURN(err);
	size_t urpc_cap_size;
	err  = frame_alloc(server_ep,BASE_PAGE_SIZE,&urpc_cap_size);
	ON_ERR_RETURN(err);
	char *urpc_data = NULL;
	err = paging_map_frame_complete(get_current_paging_state(), (void **) &urpc_data, *server_ep, NULL, NULL);
	ON_ERR_RETURN(err);
	err =  aos_rpc_init_ump_default(new_rpc,(lvaddr_t) urpc_data, BASE_PAGE_SIZE,first_half);
	ON_ERR_RETURN(err);
	err = aos_rpc_set_interface(new_rpc,get_opaque_server_interface(),OS_IFACE_N_FUNCTIONS,get_opaque_server_rpc_handlers);
	ON_ERR_RETURN(err);
	init_server_handlers(new_rpc);


	*ret_rpc = new_rpc;
	return SYS_ERR_OK;

}

/**
 * @brief create lmp server endpoint 
 *
 * @param server_ep  our endpoint cap ( to be filled in )
 * @param ret_rpc struct aos_rpc to be created and returned

 *
 * @return SYS_ERR_OK
 */

errval_t create_lmp_server_ep(struct capref* server_ep, struct aos_rpc** ret_rpc){
	errval_t err;
	err = slot_alloc(server_ep);
	ON_ERR_RETURN(err);
	struct aos_rpc* new_rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
	struct lmp_endpoint * lmp_ep;
	err = endpoint_create(256,server_ep,&lmp_ep);
	ON_ERR_RETURN(err);
	err = aos_rpc_init_lmp(new_rpc,*server_ep,NULL_CAP,lmp_ep, get_default_waitset());
	ON_ERR_RETURN(err);

	err = aos_rpc_set_interface(new_rpc,get_opaque_server_interface(),OS_IFACE_N_FUNCTIONS,get_opaque_server_rpc_handlers);
	init_server_handlers(new_rpc);

	*ret_rpc = new_rpc;
	return SYS_ERR_OK;
}


/**
 * @brief create lmp server endpoint with existing aos_rpc struct 
 *
 * @param server_ep  our endpoint cap ( to be filled in )
 * @param new_rpc existing aos_rpc struct

 *
 * @return SYS_ERR_OK
 */

errval_t create_lmp_server_ep_with_struct_aos_rpc(struct capref* server_ep, struct aos_rpc* new_rpc){
	errval_t err;
	err = slot_alloc(server_ep);
	ON_ERR_RETURN(err);
	struct lmp_endpoint * lmp_ep;
	err = endpoint_create(256,server_ep,&lmp_ep);
	ON_ERR_RETURN(err);
	err = aos_rpc_init_lmp(new_rpc,*server_ep,NULL_CAP,lmp_ep, get_default_waitset());
	ON_ERR_RETURN(err);

	err = aos_rpc_set_interface(new_rpc,get_opaque_server_interface(),OS_IFACE_N_FUNCTIONS,get_opaque_server_rpc_handlers);
	init_server_handlers(new_rpc);

	return SYS_ERR_OK;
}
/**
 * @brief register the server with init, for binding requests and indirect client calls, sets up routing entries in init
 *
 * @param name  name of server
 * @param server_rpc  struct aos_rpc implementing the server channel, has interface Opaque server interface
 * @param local_cap  local server ep cap
 *
 * @return SYS_ERR_OK
 */

errval_t establish_init_server_con(const char* name,struct aos_rpc* server_rpc, struct capref local_cap){
	errval_t err;

	struct capref remote_cap;
	err = aos_rpc_call(get_init_rpc(),INIT_MULTI_HOP_CON,name,local_cap,&remote_cap);
	ON_ERR_RETURN(err);
	server_rpc -> channel.lmp.remote_cap = remote_cap;
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

	errval_t err;
	bool success;
	err = aos_rpc_call(get_ns_rpc(), NS_DEREG_SERVER,name,&success);
	if(err_is_fail(err)){
		DEBUG_ERR(err,"Failed to call dereg server!\n");
	}
	if(success){
		remove_server(name);
		return SYS_ERR_OK;
	}
	else {
		return LIB_ERR_NAMESERVICE_INVALID_DEREG;
	}
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
	errval_t err;
	uintptr_t success;
	uintptr_t direct;
	coreid_t core_id;
	err = aos_rpc_call(get_ns_rpc(),NS_NAME_LOOKUP,name,&core_id,&direct,&success);
	ON_ERR_RETURN(err);
	if(!success){
		// DEBUG(LIB_ERR_NAMESERVICE_UNKNOWN_NAME,"Failed to find server\n");
		return LIB_ERR_NAMESERVICE_UNKNOWN_NAME;
	}

	err = nameservice_create_nschan(name,direct,core_id,nschan);
	ON_ERR_RETURN(err);
	return SYS_ERR_OK;

}


/**
 * @brief creates a channel to the a server with the same prefix as name and hasall the properites in properties
 *
 * @param name  name to lookup

 * @param properties properties to search by

 * @param chan  pointer to the chan representation to send messages to the service
 *
 * @return  SYS_ERR_OK on success, errval on failure
 */


errval_t nameservice_lookup_with_prop(const char *name,char * properties, nameservice_chan_t *nschan)
{
	errval_t err;
	uintptr_t success;
	uintptr_t direct;
	coreid_t core_id;
	if(!query_check(name)){
		return LIB_ERR_NAMESERVICE_INV_QUERY;
	}
	if(!property_check(properties)){
		return LIB_ERR_NAMESERVICE_INVALID_PROPERTY_FORMAT;
	}
	char* query_with_prop;
	err = serialize(name,properties,&query_with_prop);
	if(err_is_fail(err)){
		DEBUG_ERR(err,"Failed to serialize query\n");
	}
	char server_name[SERVER_NAME_SIZE];
	err = aos_rpc_call(get_ns_rpc(),NS_LOOKUP_PROP,query_with_prop,&core_id,&direct,&success,server_name);

	if(!success){
		return LIB_ERR_NAMESERVICE_UNKNOWN_NAME;
	}

	err = nameservice_create_nschan(server_name,direct,core_id,nschan);
	ON_ERR_RETURN(err);
	return SYS_ERR_OK;

}


/**
 * @brief sets up nschan 
 *
 * @param name  name of server connection

 * @param core_id core on which the server is running 

 * @param nschan  nameservice channel, returned
 *
 * @return  SYS_ERR_OK on success, errval on failure
 */


errval_t nameservice_create_nschan(const char *name,bool direct , coreid_t core_id, nameservice_chan_t * nschan){
	errval_t err;
	struct server_connection* serv_con = (struct server_connection*) malloc(sizeof(struct server_connection));

	strcpy(serv_con -> name,name);
	serv_con -> core_id = core_id;
	if(direct){
		serv_con -> direct = true;
		struct capref local_ep_cap;
		struct aos_rpc * new_client_server_channel;
		struct capref remote_cap;
		if(serv_con -> core_id == disp_get_core_id()){
			err = create_lmp_server_ep(&local_ep_cap,&new_client_server_channel);
			ON_ERR_RETURN(err);
			err = aos_rpc_call(get_init_rpc(),INIT_BINDING_REQUEST,name,disp_get_core_id(),serv_con -> core_id,local_ep_cap,&remote_cap);
			ON_ERR_RETURN(err);
			new_client_server_channel -> channel.lmp.remote_cap = remote_cap;
		}
		else {
			err = create_ump_server_ep(&local_ep_cap,&new_client_server_channel,true);
			ON_ERR_RETURN(err);
			err = aos_rpc_call(get_init_rpc(),INIT_BINDING_REQUEST,name,disp_get_core_id(),serv_con -> core_id,local_ep_cap,&remote_cap);
			ON_ERR_RETURN(err);
		}
		serv_con -> rpc = new_client_server_channel;

	}
	else{
		serv_con -> direct = false;
		serv_con -> rpc = get_init_rpc();
	}
	

	*nschan = (void*) serv_con; 
	return SYS_ERR_OK;
}




/**
 * @brief enumerates all entries that match an query (prefix match)
 * NOTE: will buffer overflow if buffer pass in result not large enough!
 * 
 * @param query     the query
 * @param num 		number of entries in the result array
 * @param result	an array of entries
 */
errval_t nameservice_enumerate(char *query, size_t *num, char **result )
{	

	errval_t err;
	if(!query_check(query)){
		return LIB_ERR_NAMESERVICE_INV_QUERY;
	}
	char * response = malloc(MAX_RPC_MSG_SIZE * sizeof(char)); 
	err = aos_rpc_call(get_ns_rpc(),NS_ENUM_SERVERS,query,response,num);
	ON_ERR_RETURN(err);
	size_t res_index = 1;
	*result = response;
	while(*response != '\0'){

		if(*response == ','){
			*response = '\0';
			response++;
			result[res_index] = response;
			res_index++;
		}
		response++;
	}
	return SYS_ERR_OK;
}



/**
 * @brief enumerates all entries that match an query (prefix match)
 * NOTE: will buffer overflow if buffer pass in result not large enough! 
 * @param query     the query, will prefix match this
 * @param properites all the properties to much this
 * @param num 		number of entries in the result array
 * @param result	an array of entries
 */
errval_t nameservice_enumerate_with_props(char *query,char * properties, size_t *num, char **result )
{	

	errval_t err;
	if(!query_check(query)){
		return LIB_ERR_NAMESERVICE_INV_QUERY;
	}

	if(!property_check(properties)){
		return LIB_ERR_NAMESERVICE_INVALID_PROPERTY_FORMAT;
	}

	char* query_with_prop;
	err = serialize(query,properties,&query_with_prop);
	if(err_is_fail(err)){
		DEBUG_ERR(err,"Failed to serialize query\n");
	}
	char * response_buf = malloc(MAX_RPC_MSG_SIZE * sizeof(char)); 
	err = aos_rpc_call(get_ns_rpc(),NS_ENUM_SERVER_PROPS,query_with_prop,num,response_buf);
	char * response = realloc(response_buf,strlen(response_buf));
	ON_ERR_RETURN(err);
	size_t res_index = 1;
	*result = response;
	while(*response != '\0'){

		if(*response == ','){
			*response = '\0';
			response++;
			result[res_index] = response;
			res_index++;
		}
		response++;
	}
	return SYS_ERR_OK;
}





void nameservice_reveice_handler_wrapper(struct aos_rpc * rpc,struct aos_rpc_varbytes message,struct capref tx_cap, struct aos_rpc_varbytes* response, struct capref* rx_cap, uintptr_t* response_size){


	struct srv_entry * se = (struct srv_entry *) rpc -> serv_entry;
	
	se -> recv_handler(se -> st,(void *) message.bytes,message.length,(void*)&response -> bytes,response_size,tx_cap,rx_cap);

}


void namservice_receive_handler_wrapper_direct(struct aos_rpc *rpc, struct aos_rpc_varbytes message,struct aos_rpc_varbytes * response,uintptr_t* response_size){
	struct srv_entry * se = (struct srv_entry *) rpc -> serv_entry;
	se -> recv_handler(se -> st,(void *) message.bytes,message.length,(void*)&response -> bytes,response_size,NULL_CAP,NULL);


}

void nameservice_binding_request_handler(struct aos_rpc *rpc,uintptr_t remote_core_id, struct capref remote_cap, struct capref* local_cap){
	errval_t err;
	
	struct aos_rpc *new_server_con;
	if(remote_core_id == disp_get_core_id()){
		err = create_lmp_server_ep(local_cap,&new_server_con);
		if(err_is_fail(err)){
			DEBUG_ERR(err,"create server ep");
		}
		new_server_con -> channel.lmp.remote_cap = remote_cap;
	}else{
		new_server_con = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
		char *urpc_data = NULL;
		err = paging_map_frame_complete(get_current_paging_state(), (void **) &urpc_data, remote_cap, NULL, NULL);
		if(err_is_fail(err)){
			DEBUG_ERR(err,"Failed to map ump frame into VSpace\n");
		}
		err = aos_rpc_init_ump_default(new_server_con,(lvaddr_t) urpc_data,BASE_PAGE_SIZE,false);
		if(err_is_fail(err)){
			DEBUG_ERR(err,"Failed to init ump default\n");
		}
		err = aos_rpc_set_interface(new_server_con,get_opaque_server_interface(),OS_IFACE_N_FUNCTIONS,get_opaque_server_rpc_handlers);
		if(err_is_fail(err)){
			DEBUG_ERR(err,"Failed to set interface\n");
		}
		*local_cap = remote_cap;
		init_server_handlers(new_server_con);
	}
	new_server_con -> serv_entry = rpc -> serv_entry;

}


static void remove_spaces (char* restrict str_trimmed, const char* restrict str_untrimmed)
{
  while (*str_untrimmed != '\0')
  {
    if(!isspace(*str_untrimmed))
    {
      *str_trimmed = *str_untrimmed;
      str_trimmed++;
    }
    str_untrimmed++;
  }
  *str_trimmed = '\0';
}


errval_t get_properties_size(char * properties,size_t * size){
	size_t ret_size = 1;
	// *size = 1;
	while(*properties != '\0'){
		if(*properties++ == ','){ret_size++;}
	}
	*size = ret_size;
	return SYS_ERR_OK;
}

errval_t serialize(const char * name, const char * properties,char** ret_server_data){	

	errval_t err;
	if(properties == NULL){
		return LIB_ERR_NAMESERVICE_INVALID_PROPERTY_FORMAT;
	}
	char* trimmed_name = (char * ) malloc(strlen(name)); 
	char * trimmed_prop = (char * ) malloc(strlen(properties)); 
	remove_spaces(trimmed_name,name);
	remove_spaces(trimmed_prop,properties);
	size_t ret_size;
	err = get_properties_size((char * ) properties,&ret_size);
	ON_ERR_RETURN(err);
	if(ret_size > 64){
		free(trimmed_name);
		free(trimmed_prop);
		debug_printf("We do not support more than 64 properties!\n");
		return LIB_ERR_NOT_IMPLEMENTED;
	}

	if(properties == NULL){
		*ret_server_data = (char * ) malloc(6 + strlen(trimmed_name));
		strcpy(*ret_server_data,"name=");
		strcat(*ret_server_data,trimmed_name);
		debug_printf("Here is output: %s\n",*ret_server_data);
		return SYS_ERR_OK;
	}

	size_t output_size = strlen(trimmed_name) + strlen(trimmed_prop) + 7;
	*ret_server_data = (char * ) malloc(output_size);
	strcpy(*ret_server_data,"name=");
	strcat(*ret_server_data,trimmed_name);
	strcat(*ret_server_data,",");
	strcat(*ret_server_data,trimmed_prop);
	free(trimmed_name);
	free(trimmed_prop);



	return SYS_ERR_OK;
}


errval_t deserialize_prop(const char * server_data,char *  key[],char *  value[], char**name,size_t * property_size){
	
	size_t map_index = 0;
	size_t key_index = 0;
	size_t value_index = 0;
	bool is_key = true;
	bool is_name = false;
	size_t name_index = 0;
	size_t count_props = 0;
	char * extract_name = (char * ) malloc(PROPERTY_MAX_SIZE);
	while(*server_data != ','){
		if(*server_data == '='){
			is_name = true;
			server_data++;
		}
		else{
			if(is_name){
				*(extract_name + name_index++) = *server_data++;
			}
			else{
				server_data++;
			}
		}
	}
	*(extract_name + name_index++) = '\0';
	*name = (char*) realloc(extract_name,name_index);
	server_data++;

	if(strlen(server_data ) == 0){return SYS_ERR_OK;}
	char * key_res = (char *) malloc(PROPERTY_MAX_SIZE);
	char * value_res = (char *) malloc(PROPERTY_MAX_SIZE);


	while(true){
		
		if(is_key){
			if(key_index >= PROPERTY_MAX_SIZE){
				goto end_fail;
			}
			if(*server_data == '='){
				*(key_res + (key_index++))= '\0';
				key[map_index] = key_res;
				key_res = malloc(PROPERTY_MAX_SIZE);
				is_key=false;
				key_index = 0;
				server_data++;
			}else{
				*(key_res + (key_index++))= *server_data++;
			}
		}else{
			if(value_index >= PROPERTY_MAX_SIZE){
				goto end_fail;
			}
			if(*server_data == ',' || *server_data == '\0'){
				*(value_res + (value_index++)) = '\0';
	
				count_props++;
				value[map_index++] = value_res;
				value_res = malloc(PROPERTY_MAX_SIZE);
				value_index = 0;
				is_key=true;
				if(*server_data == '\0'){goto end;}
				server_data++;
			}else{
				*(value_res + (value_index++)) = *server_data++;
			
			}
		}
	}
	free(key_res);
	free(value_res);
	return SYS_ERR_OK;

end:
	free(key_res);
	free(value_res);
	*property_size = count_props;
	
	return SYS_ERR_OK;
end_fail:
	free(key_res);
	free(value_res);
	return LIB_ERR_NAMESERVICE_INVALID_PROPERTY_FORMAT;
}


void init_server_handlers(struct aos_rpc* server_rpc){
	aos_rpc_register_handler(server_rpc,OS_IFACE_MESSAGE,&nameservice_reveice_handler_wrapper);
	aos_rpc_register_handler(server_rpc,OS_IFACE_BINDING_REQUEST,&nameservice_binding_request_handler);
	aos_rpc_register_handler(server_rpc,OS_IFACE_DIRECT_MESSAGE,&namservice_receive_handler_wrapper_direct);
}



errval_t nameservice_get_props(const char* name, char ** response){
	errval_t err;
	char *response_buf = (char *) malloc (PROPERTY_MAX_SIZE * N_PROPERTIES);
	err = aos_rpc_call(get_ns_rpc(),NS_GET_SERVER_PROPS,name,response_buf);
	ON_ERR_RETURN(err);
	*response = realloc(response_buf,strlen(response_buf));
	return SYS_ERR_OK;
}

errval_t nameservice_get_pid(const char* name, domainid_t* resp_pid){
	errval_t err = aos_rpc_call(get_ns_rpc(),NS_GET_SERVER_PID,name,resp_pid);
	ON_ERR_RETURN(err);
	if(*resp_pid == 0xffffffff){
		return LIB_ERR_NAMESERVICE_UNKNOWN_NAME;
	}
	return SYS_ERR_OK;
}



bool name_check(const char*name){
	regex_t regex_name;
	int reti = regcomp(&regex_name,"^/([a-z]|[A-Z])+([a-z]|[A-Z]|[0-9])*(/([a-z]|[A-Z])+([a-z]|[A-Z]|[0-9])*)*$",REG_EXTENDED);
	reti = regexec(&regex_name,name,0,NULL,0);
	return !reti;
}

bool query_check(const char*query){
	regex_t regex_name;
	int reti_name = regcomp(&regex_name,"^(/([a-z]|[A-Z])+([a-z]|[A-Z]|[0-9])*)*/$",REG_EXTENDED);
	reti_name = regexec(&regex_name,query,0,NULL,0);
	return !reti_name;
}


bool property_check(const char * properties){
	regex_t regex_name;
	int reti = regcomp(&regex_name,"^(([a-z]|[A-Z])*=([a-z]|[A-Z]|[0-9]|:|-)*)(,([a-z]|[A-Z])*=([a-z]|[A-Z]|[0-9]|:|-)*)*$",REG_EXTENDED);
	if(reti){
		debug_printf("Failed to compile!\n");
	}
	reti = regexec(&regex_name,properties,0,NULL,0);
	return !reti;
}
// we need this because the parser cannot read "="
bool property_check_terminal(const char * properties){
	regex_t regex_name;
	int reti = regcomp(&regex_name,"^(([a-z]|[A-Z])*%([a-z]|[A-Z]|[0-9]|:|-)*)(,([a-z]|[A-Z])*%([a-z]|[A-Z]|[0-9]|:|-)*)*$",REG_EXTENDED);
	if(reti){
		debug_printf("Failed to compile!\n");
	}
	reti = regexec(&regex_name,properties,0,NULL,0);
	return !reti;
}
