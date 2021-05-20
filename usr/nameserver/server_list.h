#ifndef SERVER_LIST_H_
#define SERVER_LIST_H_


#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/nameserver.h>

size_t n_servers; 
struct server_list* servers;

struct server_list {
    struct server_list* next;
    char name[SERVER_NAME_SIZE];
    domainid_t pid;
    coreid_t core_id;
    struct capref end_point;
    bool ump;
    char * key[64];
    char * value[64];

};



errval_t add_server(struct server_list* new_server);
errval_t find_server_by_name(char * name, struct server_list ** ret_serv);
void find_servers_by_prefix(const char* name, char* response, size_t * resp_size);
bool verify_name(const char* name); // TODO 

void remove_server(struct server_list* del_server);
void print_server_list(void);
bool prefix_match(char* name, char* server_name);

#endif