#ifndef PROCESS_MANAGER_INTERFACES_H
#define PROCESS_MANAGER_INTERFACES_H

#include <aos/aos_rpc.h>

struct aos_rpc_interface *get_pm_interface(void);

enum {
    PM_IFACE_REGISTER_PROCESS = AOS_RPC_MSG_TYPE_START,

    PM_IFACE_N_FUNCTIONS, // <- count -- must be last
};


#endif // PROCESS_MANAGER_INTERFACES_H
