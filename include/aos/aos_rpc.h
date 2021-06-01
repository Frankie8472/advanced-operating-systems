/**
 * \file
 * \brief RPC Bindings for AOS
 */

/*
 * Copyright (c) 2013-2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. Attn: Systems Group.
 */

#ifndef _LIB_BARRELFISH_AOS_MESSAGES_H
#define _LIB_BARRELFISH_AOS_MESSAGES_H

#define AOS_RPC_MAX_FUNCTION_ARGUMENTS 8

#include <aos/aos.h>
#include <aos/ump_chan.h>

#define AOS_RPC_RETURN_BIT 0x1000000
#define DEFAULT_TIMEOUT 20000000000

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

enum aos_rpc_backend {
    AOS_RPC_LMP = 1,
    AOS_RPC_UMP = 2,
};

/**
 * \brief different functions to call for rpc
 */
typedef enum aos_rpc_msg_type {
    AOS_RPC_INITIATE = 0,
    AOS_RPC_SEND_NUMBER,
    AOS_RPC_SEND_STRING,
    AOS_RPC_SEND_VARBYTES,
    AOS_RPC_PUTCHAR,
    AOS_RPC_GETCHAR, 
    AOS_RPC_BINDING_REQUEST,
    AOS_RPC_ROUNDTRIP, ///< rpc call that does nothing, for benchmarking
    AOS_RPC_MSG_TYPE_START,

    AOS_RPC_REQUEST_RAM,
    AOS_RPC_SETUP_PAGE,  
    AOS_RPC_REQUEST_FOREIGN_RAM,
    AOS_RPC_PROC_SPAWN_REQUEST,
    AOS_RPC_FOREIGN_SPAWN,
    AOS_RPC_SET_READ,
    AOS_RPC_FREE_READ,
    AOS_RPC_REGISTER_PROCESS,
    AOS_RPC_GET_PROC_NAME,
    AOS_RPC_GET_PROC_LIST,
    AOS_RPC_MEM_SERVER_REQ,
    AOS_RPC_GET_PROC_CORE,
    AOS_RPC_MAX_MSG_TYPES, // needs to be last
} msg_type_t;


/**
 * \brief possible argument/return types for rpc calls
 */
enum aos_rpc_argument_type {
    AOS_RPC_NO_TYPE = 0,
    AOS_RPC_WORD,
    AOS_RPC_SHORTSTR, ///< four word string (32 chars) (not implemented)
    AOS_RPC_STR, ///< longer string in shared page (not implemented)
    AOS_RPC_VARSTR,
    AOS_RPC_VARBYTES,
    AOS_RPC_CAPABILITY
};


#define AOS_RPC_SHORTSTR_LENGTH 32

struct aos_rpc_varbytes
{
    uintptr_t length;
    char *bytes;
};


/**
 * \brief containing info for rpc (un)marshalling
 */
struct aos_rpc_function_binding
{
    int                             msg_type;
    uint16_t                        n_args;
    uint16_t                        n_rets;
    void                            *buf_page;
    void                            *buf_page_remote;
    char                            binding_name[32];
    enum aos_rpc_argument_type      args[AOS_RPC_MAX_FUNCTION_ARGUMENTS];
    enum aos_rpc_argument_type      rets[AOS_RPC_MAX_FUNCTION_ARGUMENTS];
};


struct aos_rpc_interface
{
    size_t n_bindings;
    struct aos_rpc_function_binding *bindings;
};


/* An RPC binding, which may be transported over LMP or UMP. */
struct aos_rpc {
    enum aos_rpc_backend backend;

    ///
    /// \brief sends with each call also the endpoint capability to which
    ///        it should return the call to.
    /// \note  This is dangerous when used with multi-msg calls, as message receiving might interleave
    ///
    bool lmp_server_mode;

    union backend_channel {
        struct lmp_chan lmp;
        struct ump_chan ump;
    } channel;

    const struct aos_rpc_interface *interface;

    void* serv_entry;
    struct waitset *waitset;

    size_t n_handlers;
    void **handlers;
    uint64_t timeout;
};

errval_t aos_rpc_set_interface(struct aos_rpc *rpc, struct aos_rpc_interface *interface, size_t n_handlers, void **handlers);

errval_t aos_rpc_init_lmp(struct aos_rpc *rpc, struct capref self_ep, struct capref end_ep, struct lmp_endpoint *lmp_ep, struct waitset *waitset);
errval_t aos_rpc_init_ump_default(struct aos_rpc *rpc, lvaddr_t shared_page, size_t shared_page_size, bool first_half);

errval_t aos_rpc_free(struct aos_rpc *rpc);

errval_t aos_rpc_initialize_binding(struct aos_rpc_interface *interface, const char *name, int msg_type,
                                    int n_args, int n_rets, ...);

errval_t aos_rpc_call(struct aos_rpc *rpc, enum aos_rpc_msg_type binding, ...);

errval_t aos_rpc_register_handler(struct aos_rpc *rpc, enum aos_rpc_msg_type binding,
                                  void* handler);

void aos_rpc_on_lmp_message(void *rpc);


void aos_rpc_on_ump_message(void *arg);

errval_t aos_rpc_send_number(struct aos_rpc *chan, uintptr_t val);

errval_t aos_rpc_send_string(struct aos_rpc *chan, const char *string);

errval_t aos_rpc_get_ram_cap(struct aos_rpc *chan, size_t bytes,
                             size_t alignment, struct capref *retcap,
                             size_t *ret_bytes);

errval_t aos_rpc_serial_getchar(struct aos_rpc *chan, char *retc);

errval_t aos_rpc_serial_putchar(struct aos_rpc *chan, char c);

errval_t aos_rpc_process_spawn(struct aos_rpc *chan, char *cmdline,
                               coreid_t core, domainid_t *newpid);

errval_t aos_rpc_process_get_name(struct aos_rpc *chan, domainid_t pid,
                                  char **name);

errval_t aos_rpc_process_get_all_pids(struct aos_rpc *chan,
                                      domainid_t **pids, size_t *pid_count);

errval_t aos_rpc_get_terminal_input(struct aos_rpc *chan, char* buf,size_t le);


struct aos_rpc *aos_rpc_get_init_channel(void);

struct aos_rpc *aos_rpc_get_memory_channel(void);

struct aos_rpc *aos_rpc_get_process_channel(void);

struct aos_rpc *aos_rpc_get_serial_channel(void);

errval_t aos_rpc_request_foreign_ram(struct aos_rpc * rpc, size_t size,struct capref *ret_cap,size_t * ret_size);

void aos_rpc_set_timeout(struct aos_rpc * rpc, uint64_t timeout);



#endif // _LIB_BARRELFISH_AOS_MESSAGES_H
