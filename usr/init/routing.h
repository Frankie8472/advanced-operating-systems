#ifndef ROUTING_H_
#define ROUTING_H_

#include <errno.h>
#include <aos/aos_rpc.h>

#include <aos/nameserver.h>

struct hashtable* routing_ht;

struct routing_entry {
    // struct routing_entry* next;
    char name[SERVER_NAME_SIZE];
    struct aos_rpc* rpc;
};

errval_t add_routing_entry(struct routing_entry * re);
errval_t get_routing_entry_by_name(const char* name,struct routing_entry** ret_re);
errval_t remove_routing_entry(const char * name);


#endif //ROUTING