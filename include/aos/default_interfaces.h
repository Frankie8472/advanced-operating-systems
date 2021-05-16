#ifndef LIB_AOS_DEFAULT_INTERFACES_H
#define LIB_AOS_DEFAULT_INTERFACES_H

#include <aos/aos_rpc.h>

struct aos_rpc_interface *get_init_interface(void);
struct aos_rpc_interface *get_dispatcher_interface(void);
struct aos_rpc_interface *get_write_interface(void);
struct aos_rpc_interface *get_memory_server_interface(void);

void initialize_initiate_handler(struct aos_rpc *rpc);

enum {
    INIT_IFACE_GET_RAM = AOS_RPC_MSG_TYPE_START,
    INIT_IFACE_SPAWN,
    INIT_IFACE_SPAWN_EXTENDED,      ///< same as spawn but with more detailed parameters 
    INIT_IFACE_GET_PROCESS_LIST,

    INIT_IFACE_N_FUNCTIONS, // <- count -- must be last
};


enum {
    DISP_IFACE_BINDING,
    DISP_IFACE_REBIND,
    DISP_IFACE_SET_STDOUT,
    DISP_IFACE_GET_STDIN,

    DISP_IFACE_N_FUNCTIONS, // <- count -- must be last
};


enum {
    WRITE_IFACE_WRITE_VARBYTES,
    WRITE_IFACE_N_FUNCTIONS, // <- count -- must be last
};


enum {
    MM_IFACE_GET_RAM = AOS_RPC_MSG_TYPE_START,
    MM_IFACE_N_FUNCTIONS, // <- count -- must be last
};


struct spawn_request_header
{
    int argc;
    int envc;
};

struct spawn_request_arg
{
    size_t length;
    char str[0];
};


#endif // LIB_AOS_DEFAULT_INTERFACES_H
