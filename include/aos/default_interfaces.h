#ifndef LIB_AOS_DEFAULT_INTERFACES_H
#define LIB_AOS_DEFAULT_INTERFACES_H

#include <aos/aos_rpc.h>

struct aos_rpc_interface *get_init_interface(void);
struct aos_rpc_interface *get_dispatcher_interface(void);
struct aos_rpc_interface *get_write_interface(void);
struct aos_rpc_interface *get_memory_server_interface(void);
struct aos_rpc_interface *get_nameserver_interface(void);
struct aos_rpc_interface * get_opaque_server_interface(void);

void initialize_initiate_handler(struct aos_rpc *rpc);

#define MOD_NOT_FOUND -1
#define COREID_INVALID -2

enum {
    INIT_IFACE_SPAWN = AOS_RPC_MSG_TYPE_START,
    INIT_IFACE_SPAWN_EXTENDED,      ///< same as spawn but with more 
    INIT_NAMESERVER_ON,
    INIT_REG_NAMESERVER,
    INIT_REG_SERVER,
    INIT_NAME_LOOKUP,
    INIT_CLIENT_CALL,
    INIT_CLIENT_CALL1,
    INIT_CLIENT_CALL2,
    INIT_CLIENT_CALL3,
    INIT_MULTI_HOP_CON,
    INIT_BINDING_REQUEST,
    INIT_IFACE_N_FUNCTIONS, // <- count -- must be last
};


enum {
    DISP_IFACE_BINDING = AOS_RPC_MSG_TYPE_START,
    DISP_IFACE_REBIND,
    DISP_IFACE_SET_STDOUT,
    DISP_IFACE_GET_STDIN,
    DISP_IFACE_TERMINATE,

    DISP_IFACE_N_FUNCTIONS, // <- count -- must be last
};


enum {
    WRITE_IFACE_WRITE_VARBYTES = AOS_RPC_MSG_TYPE_START,
    WRITE_IFACE_N_FUNCTIONS, // <- count -- must be last
};


enum {
    MM_IFACE_GET_RAM = AOS_RPC_MSG_TYPE_START,
    MM_IFACE_N_FUNCTIONS, // <- count -- must be last
};



enum {
    NS_REG_SERVER = AOS_RPC_MSG_TYPE_START,
    NS_DEREG_SERVER,
    NS_ENUM_SERVERS,
    NS_GET_PROC_NAME,
    NS_GET_PROC_CORE,
    NS_GET_PROC_LIST,
    NS_DEREG_PROCESS,
    NS_GET_PID,
    NS_IFACE_N_FUNCTIONS,

};

enum {
    OS_IFACE_MESSAGE = AOS_RPC_MSG_TYPE_START,
    OS_IFACE_DIRECT_MESSAGE,
    OS_IFACE_BINDING_REQUEST,
    OS_IFACE_N_FUNCTIONS,
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
