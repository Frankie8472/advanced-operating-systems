/**
 * \file nameservice.h
 * \brief 
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <aos/aos.h>
#include <aos/waitset.h>
#include <aos/nameserver.h>
#include <aos/aos_rpc.h>
#include <stdarg.h>
#include <aos/default_interfaces.h>


#include <hashtable/hashtable.h>

static void *get_opaque_server_rpc_handlers[OS_IFACE_N_FUNCTIONS];
// * server_table = create_hashtable();




struct srv_entry {
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


	errval_t err;
	// debug_printf("here is channel address: 0x%lx\n",chan);
	struct server_connection *serv_con = (struct server_connection *) chan;


	struct capref response_cap;
	slot_alloc(&response_cap);
	if(serv_con -> ump){
		if(!capref_is_null(tx_cap) && !capref_is_null(rx_cap)){
			debug_printf("Trying to send or recv cap over a ump server connection!(impossible to do!)\n");
			return LIB_ERR_NOT_IMPLEMENTED;
		}
		return LIB_ERR_NOT_IMPLEMENTED;
	}else{

		char * payload = (char * ) malloc(bytes + strlen(serv_con -> name) + 2);
		strcpy(payload,serv_con -> name);
		strcat(payload,"?");
		strcat(payload,(char * ) message);
		// debug_printf("Client call with core: %d message: %s\n",serv_con -> core_id,message );
		err = aos_rpc_call(serv_con -> rpc,INIT_CLIENT_CALL,serv_con -> core_id,payload,tx_cap,(char *) *response,&response_cap);
		ON_ERR_RETURN(err);
		*response_bytes = strlen(*response);

		// debug_printf("Response: %s\n",(char*) *response);

	}
	return SYS_ERR_OK;
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
	
	return nameservice_register_properties(name,recv_handler,st,false,NULL);
}


errval_t nameservice_register_properties(const char * name,nameservice_receive_handler_t recv_handler, void * st, bool ump,const char * properties){


	errval_t err;


	char* server_data;
	err = serialize(name,properties,&server_data);
	ON_ERR_RETURN(err);
	// create and add srv_entry
	struct srv_entry* new_srv_entry = (struct srv_entry *) malloc(sizeof(struct srv_entry));
	new_srv_entry -> name = name;
	new_srv_entry -> recv_handler = recv_handler;
	new_srv_entry -> st = st;


	struct aos_rpc *new_rpc;
	struct capref server_ep;
	if(ump){
		err = create_ump_server_ep(&server_ep,&new_rpc);
	}else{
		err = create_lmp_server_ep(&server_ep,&new_rpc);
	}
	ON_ERR_RETURN(err);


	// char buffer[512];

	// debug_print_cap_at_capref(buffer,512,server_ep);
	// debug_printf("Cap : %s\n",buffer);


	// char buffer[512];
	// struct capability cap;
	// invoke_cap_identify(server_ep,&cap);
	// debug_print_cap(buffer,512,&cap);
	// debug_printf("Register with name : %s \n",server_data);

	char buf[512];
	err = aos_rpc_call(get_init_rpc(),INIT_REG_SERVER,disp_get_domain_id(),disp_get_core_id(),server_data,server_ep,buf);
	ON_ERR_RETURN(err);
	//TODO: rich properties
	//TODO: lmp ep

	if(!ump){
		establish_init_server_con(name,new_rpc,server_ep);
	}

	new_rpc -> serv_entry = (void*) new_srv_entry;

	return SYS_ERR_OK;
}

errval_t create_ump_server_ep(struct capref* server_ep,struct aos_rpc** ret_rpc){
	errval_t err;
	struct aos_rpc* new_rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
	err = slot_alloc(server_ep);
	ON_ERR_RETURN(err);
	size_t urpc_cap_size;
	err  = frame_alloc(server_ep,BASE_PAGE_SIZE,&urpc_cap_size);
	ON_ERR_RETURN(err);
	char *urpc_data = NULL;
	err = paging_map_frame_complete(get_current_paging_state(), (void **) &urpc_data, *server_ep, NULL, NULL);
	err =  aos_rpc_init_ump_default(new_rpc,(lvaddr_t) urpc_data, BASE_PAGE_SIZE,true);
	ON_ERR_RETURN(err);
	err = aos_rpc_set_interface(new_rpc,get_opaque_server_interface(),OS_IFACE_N_FUNCTIONS,get_opaque_server_rpc_handlers);
	ON_ERR_RETURN(err);
	init_server_handlers(new_rpc);


	*ret_rpc = new_rpc;
	return SYS_ERR_OK;

}

errval_t create_lmp_server_ep(struct capref* server_ep, struct aos_rpc** ret_rpc){
	errval_t err;
	err = slot_alloc(server_ep);
	ON_ERR_RETURN(err);
	struct aos_rpc* new_rpc = (struct aos_rpc*) malloc(sizeof(struct aos_rpc));
	// new_rpc -> lmp_server_mode  = true; // NOTE: unsure?
	struct lmp_endpoint * lmp_ep;
	err = endpoint_create(256,server_ep,&lmp_ep);
	ON_ERR_RETURN(err);
	err = aos_rpc_init_lmp(new_rpc,*server_ep,NULL_CAP,lmp_ep, get_default_waitset());
	ON_ERR_RETURN(err);

	err = aos_rpc_set_interface(new_rpc,get_opaque_server_interface(),OS_IFACE_N_FUNCTIONS,get_opaque_server_rpc_handlers);
	init_server_handlers(new_rpc);

	// char buffer[512];

	// debug_print_cap_at_capref(buffer,512,new_rpc -> channel.lmp.local_cap);
	// debug_printf("Cap : %s\n",buffer);

	*ret_rpc = new_rpc;
	return SYS_ERR_OK;
}

errval_t establish_init_server_con(const char* name,struct aos_rpc* server_rpc, struct capref local_cap){
	errval_t err;
	debug_printf("establish init server con\n");

	struct capref remote_cap;


	char buffer[512];

	debug_print_cap_at_capref(buffer,512,local_cap);
	debug_printf("Cap : %s\n",buffer);
	
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


	errval_t err;
	struct capref server_ep;
	// err = slot_alloc(&server_ep);
	// ON_ERR_RETURN(err);
	bool ump;
	coreid_t core_id;
	err = aos_rpc_call(get_init_rpc(),INIT_NAME_LOOKUP,name,&core_id,&ump,&server_ep);
	ON_ERR_RETURN(err);

	struct server_connection* serv_con = (struct server_connection*) malloc(sizeof(struct server_connection));
	const char * con_name = (const char *) malloc(sizeof(strlen(name) + 1));
	strcpy((char*)con_name,(char*)name);  
	serv_con -> name = con_name;
	serv_con -> core_id = core_id;
	if(ump){
		serv_con -> ump = true;
		return LIB_ERR_NOT_IMPLEMENTED;
	}
	else{
		serv_con -> ump = false;
		serv_con -> rpc = get_init_rpc();
	}
	

	*nschan = (void*) serv_con; 
	return SYS_ERR_OK;
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



void nameservice_reveice_handler_wrapper(struct aos_rpc * rpc,char*  message,struct capref tx_cap, char * response, struct capref* rx_cap){

	
	struct srv_entry * se = (struct srv_entry *) rpc -> serv_entry;
	size_t response_bytes;
	char * response_buffer = (char * ) malloc(1024); //TODO make varstrlarger
	se -> recv_handler(se -> st,(void *) message,strlen(message),(void*)&response_buffer,&response_bytes,tx_cap,rx_cap);
	debug_printf("Response in wrapper : %s\n",response_buffer);
	strcpy(response,response_buffer);
	// free(response_buffer);

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

	// if()
	// TODO: Regex check
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

	// TODO: Regex check for properties
	// debug_printf("Here is output: %s\n",*ret_server_data);
	// debug_printf("outsize : %d\n",output_size);

	return SYS_ERR_OK;
}


errval_t deserialize_prop(const char * server_data,char *  key[],char *  value[], char**name){
	
	size_t map_index = 0;
	size_t key_index = 0;
	size_t value_index = 0;
	bool is_key = true;
	bool is_name = false;
	size_t name_index = 0;

	char * extract_name = (char * ) malloc(512);
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

	char * key_res = (char *) malloc(512);
	char * value_res = (char *) malloc(512);


	while(true){
		
		if(is_key){
			if(*server_data == '='){
				*(key_res + (key_index++))= '\0';
				// key[map_index] = (char*) realloc(key_res,key_index);
				key[map_index] = key_res;
				// debug_printf("0x%lx -> %s\n",key_res,key[map_index]);
				key_res = malloc(512);
				is_key=false;
				key_index = 0;
				server_data++;
			}else{
				// debug_printf("%c\n",*server_data);
				*(key_res + (key_index++))= *server_data++;
			}
		}else{
			if(*server_data == ',' || *server_data == '\0'){
				*(value_res + (value_index++)) = '\0';
				// key[map_index++] = (char*) realloc(value_res,value_index);
				value[map_index++] = value_res;
				value_res = malloc(512);
				value_index = 0;
				is_key=true;
				if(*server_data == '\0'){return SYS_ERR_OK;}
				server_data++;
			}else{
				// debug_printf("%c\n",*server_data);
				*(value_res + (value_index++)) = *server_data++;
			
			}
		}
	}

	// free(key_res);
	// free(value_res);

	return SYS_ERR_OK;
}


void init_server_handlers(struct aos_rpc* server_rpc){
	aos_rpc_register_handler(server_rpc,OS_IFACE_MESSAGE,&nameservice_reveice_handler_wrapper);
}