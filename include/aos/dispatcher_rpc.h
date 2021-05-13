#ifndef LIB_AOS_DISPATCHER_RPC_H
#define LIB_AOS_DISPATCHER_RPC_H

#include <aos/aos_rpc.h>

extern struct aos_rpc stdin_rpc;
extern struct aos_rpc stdout_rpc;
extern struct aos_rpc dispatcher_rpc;

errval_t init_dispatcher_rpcs(void);


#endif // LIB_AOS_DISPATCHER_RPC_H
