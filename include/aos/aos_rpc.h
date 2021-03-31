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

#define AOS_RPC_RETURN_BIT 0x1000000
#define AOS_RPC_MAX_MSG_TYPES 32

typedef enum aos_rpc_msg_type {
    AOS_RPC_INITIATE = 1,
    AOS_RPC_SEND_NUMBER,
    AOS_RPC_SEND_STRING,
    AOS_RPC_REQUEST_RAM,
    AOS_RPC_SETUP_PAGE,
    AOS_RPC_PROC_SPAWN_REQUEST,
    AOS_RPC_PUTCHAR,
    AOS_RPC_GETCHAR,
    AOS_RPC_SET_READ,
    AOS_RPC_FREE_READ,
    AOS_RPC_TERMINAL_READ,
    AOS_RPC_TERMINAL_WRITE,
} msg_type_t;



enum aos_rpc_argument_type {
    AOS_RPC_NO_TYPE = 0,
    AOS_RPC_WORD,
    AOS_RPC_STR,
    AOS_RPC_BYTES,
    AOS_RPC_CAPABILITY
};

struct aos_rpc_function_binding
{
    enum aos_rpc_msg_type           port;
    uint16_t                        n_args;
    uint16_t                        n_rets;
    void                            *buf_page;
    void                            *buf_page_remote;
    bool                            calling_simple;
    bool                            returning_simple;
    enum aos_rpc_argument_type      args[AOS_RPC_MAX_FUNCTION_ARGUMENTS];
    enum aos_rpc_argument_type      rets[AOS_RPC_MAX_FUNCTION_ARGUMENTS];
};

struct aos_rpc_function_handler
{
    void *func_ptr;
};


/* An RPC binding, which may be transported over LMP or UMP. */
struct aos_rpc {
    struct lmp_chan channel;

    size_t n_bindings;
    struct aos_rpc_function_binding bindings[AOS_RPC_MAX_MSG_TYPES];
    void *handlers[AOS_RPC_MAX_MSG_TYPES];
    // TODO(M3): Add state
};

/**
 * \brief Initialize an aos_rpc struct.
 */
errval_t aos_rpc_init(struct aos_rpc *rpc, struct capref self_ep, struct capref end_ep, struct lmp_endpoint *lmp_ep);

errval_t aos_rpc_initialize_binding(struct aos_rpc *rpc, enum aos_rpc_msg_type binding,
                                    int n_args, int n_rets, ...);

errval_t aos_rpc_call(struct aos_rpc *rpc, enum aos_rpc_msg_type binding, ...);

errval_t aos_rpc_register_handler(struct aos_rpc *rpc, enum aos_rpc_msg_type binding,
                                  void* handler);

void aos_rpc_test(uintptr_t x, uintptr_t y, uintptr_t z, uintptr_t w, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d);

void aos_rpc_on_message(void *rpc);

/**
 * \brief Send a number.
 */
errval_t aos_rpc_send_number(struct aos_rpc *chan, uintptr_t val);

/**
 * \brief Send a string.
 */
errval_t aos_rpc_send_string(struct aos_rpc *chan, const char *string);


/**
 * \brief Request a RAM capability with >= request_bits of size over the given
 * channel.
 */
errval_t aos_rpc_get_ram_cap(struct aos_rpc *chan, size_t bytes,
                             size_t alignment, struct capref *retcap,
                             size_t *ret_bytes);


/**
 * \brief Get one character from the serial port
 */
errval_t aos_rpc_serial_getchar(struct aos_rpc *chan, char *retc);


/**
 * \brief Send one character to the serial port
 */
errval_t aos_rpc_serial_putchar(struct aos_rpc *chan, char c);

/**
 * \brief Request that the process manager start a new process
 * \arg cmdline the name of the process that needs to be spawned (without a
 *           path prefix) and optionally any arguments to pass to it
 * \arg newpid the process id of the newly-spawned process
 */
errval_t aos_rpc_process_spawn(struct aos_rpc *chan, char *cmdline,
                               coreid_t core, domainid_t *newpid);


/**
 * \brief Get name of process with the given PID.
 * \arg pid the process id to lookup
 * \arg name A null-terminated character array with the name of the process
 * that is allocated by the rpc implementation. Freeing is the caller's
 * responsibility.
 */
errval_t aos_rpc_process_get_name(struct aos_rpc *chan, domainid_t pid,
                                  char **name);


/**
 * \brief Get PIDs of all running processes.
 * \arg pids An array containing the process ids of all currently active
 * processes. Will be allocated by the rpc implementation. Freeing is the
 * caller's  responsibility.
 * \arg pid_count The number of entries in `pids' if the call was successful
 */
errval_t aos_rpc_process_get_all_pids(struct aos_rpc *chan,
                                      domainid_t **pids, size_t *pid_count);



errval_t aos_rpc_get_terminal_input(struct aos_rpc *chan, char* buf,size_t le);
/**
 * \brief Returns the RPC channel to init.
 */
struct aos_rpc *aos_rpc_get_init_channel(void);

/**
 * \brief Returns the channel to the memory server
 */
struct aos_rpc *aos_rpc_get_memory_channel(void);

/**
 * \brief Returns the channel to the process manager
 */
struct aos_rpc *aos_rpc_get_process_channel(void);

/**
 * \brief Returns the channel to the serial console
 */
struct aos_rpc *aos_rpc_get_serial_channel(void);

#endif // _LIB_BARRELFISH_AOS_MESSAGES_H
