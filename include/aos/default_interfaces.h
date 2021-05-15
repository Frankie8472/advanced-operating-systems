#ifndef LIB_AOS_DEFAULT_INTERFACES_H
#define LIB_AOS_DEFAULT_INTERFACES_H

#include <aos/aos_rpc.h>

struct aos_rpc_interface *get_init_interface(void);
struct aos_rpc_interface *get_dispatcher_interface(void);
struct aos_rpc_interface *get_write_interface(void);
struct aos_rpc_interface *get_memory_server_interface(void);
struct aos_rpc_interface *get_nameserver_interface(void);

void initialize_initiate_handler(struct aos_rpc *rpc);

enum {
    INIT_IFACE_SPAWN = AOS_RPC_MSG_TYPE_START,
    //FORWARDING TODO
    INIT_NAMESERVER_ON,
    INIT_REG_NAMESERVER,
    INIT_IFACE_N_FUNCTIONS, // <- count -- must be last
};


enum {
    DISP_IFACE_BINDING = AOS_RPC_MSG_TYPE_START,
    DISP_IFACE_REBIND,
    DISP_IFACE_SET_STDOUT,
    DISP_IFACE_GET_STDIN,

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
    NS_GET_PROC_NAME,
    NS_GET_PROC_CORE,
    NS_GET_PROC_LIST,
    NS_GET_PID,
    NS_IFACE_N_FUNCTIONS,

};

enum {
    OS_IFACE_MESSAGE = AOS_RPC_MSG_TYPE_START,
    OS_IFACE_N_FUNCTIONS,
};


#endif // LIB_AOS_DEFAULT_INTERFACES_H
