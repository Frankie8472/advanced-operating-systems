#ifndef LIB_AOS_DISPATCHER_RPC_H
#define LIB_AOS_DISPATCHER_RPC_H

#include <aos/aos_rpc.h>
#include <aos/aos_datachan.h>

extern struct aos_datachan stdin_chan;
extern struct aos_datachan stdout_chan;
extern struct aos_rpc dispatcher_rpc;

errval_t init_dispatcher_rpcs(void);
errval_t init_nameserver_rpc(char * name);

#endif // LIB_AOS_DISPATCHER_RPC_H
