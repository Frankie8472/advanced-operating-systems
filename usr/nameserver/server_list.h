#ifndef SERVER_LIST_H_
#define SERVER_LIST_H_


#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/nameserver.h>

size_t n_servers; 
struct server_list* servers;

struct hashtable* server_ht;

struct server_list {
    struct server_list* next;
    char name[SERVER_NAME_SIZE];
    domainid_t pid;
    coreid_t core_id;
    bool direct;
    char * key[N_PROPERTIES];
    char * value[N_PROPERTIES];
    size_t n_properties;
    bool marked;

};


errval_t add_server(struct server_list* new_server);
errval_t find_server_by_name(char * name, struct server_list ** ret_serv);
errval_t find_server_by_name_and_property(const char * name, char*  keys[],char*  values[],size_t prop_size,struct server_list ** ret_serv);
void find_servers_by_prefix(const char* name, char* response, size_t * resp_size);
void find_servers_by_prefix_and_prop(const char* name,char*  keys[],char*  values[],size_t prop_size , char* response,size_t * resp_size);

void remove_server(struct server_list* del_server);
void free_server(struct server_list* server);

bool property_match(struct server_list* server, char *  keys[],char* values[], size_t prop_size);
bool prefix_match(char* name, char* server_name);
void print_server_list(void);
#endif