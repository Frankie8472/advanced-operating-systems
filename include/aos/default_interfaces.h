#ifndef LIB_AOS_DEFAULT_INTERFACES_H
#define LIB_AOS_DEFAULT_INTERFACES_H

#include <aos/aos_rpc.h>

struct aos_rpc_interface *get_init_interface(void);
struct aos_rpc_interface *get_dispatcher_interface(void);
struct aos_rpc_interface *get_write_interface(void);

void initialize_initiate_handler(struct aos_rpc *rpc);

enum {
    INIT_IFACE_INITIATE,
    INIT_IFACE_GET_RAM,
    INIT_IFACE_SPAWN,
    INIT_IFACE_GET_PROCESS_LIST,

    INIT_IFACE_N_FUNCTIONS, // <- count -- must be last
};


enum {
    DISP_IFACE_BINDING_REQUEST,
    DISP_IFACE_SET_STDOUT,
    DISP_IFACE_GET_STDIN,

    DISP_IFACE_N_FUNCTIONS, // <- count -- must be last
};


enum {
    WRITE_IFACE_WRITE_VARBYTES,
    WRITE_IFACE_N_FUNCTIONS, // <- count -- must be last
};


#endif // LIB_AOS_DEFAULT_INTERFACES_H
