#ifndef SERVER_LIST_H_
#define SERVER_LIST_H_


#include <aos/aos.h>
#include <aos/aos_rpc.h>


struct server_list* servers;

struct server_list {
    struct server_list* next;
    const char* name;
    domainid_t pid;
    coreid_t core_id;
    struct capref end_point;
    bool ump;
    char * key[64];
    char * value[64];

};



errval_t add_server(struct server_list* new_server);
errval_t find_server_by_name(char * name, struct server_list ** ret_serv);
bool verify_name(const char* name); // TODO 

void remove_server(struct server_list* del_server);
void print_server_list(void);

#endif