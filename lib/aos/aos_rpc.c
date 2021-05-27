/**
 * \file
 * \brief RPC Bindings for AOS
 */

/*
 * Copyright (c) 2013-2016, ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached license file.
 * if you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Universitaetstr. 6, CH-8092 Zurich. attn: systems group.
 */

#include <aos/aos.h>
#include <aos/aos_rpc.h>
#include <aos/kernel_cap_invocations.h>
#include <arch/aarch64/aos/lmp_chan_arch.h>
#include <stdarg.h>
#include <aos/kernel_cap_invocations.h>
#include <aos/default_interfaces.h>
#include "init.h"



struct lmp_msg_info {
    struct lmp_recv_msg msg;
    int word_index;
    struct capref cap;
    bool cap_taken;
};


/* ================== Function Declarations ================== */

static void aos_rpc_setup_page_handler(struct aos_rpc* rpc, uintptr_t msg_type, uintptr_t frame_size, struct capref frame);
static errval_t aos_rpc_call_ump(struct aos_rpc *rpc, enum aos_rpc_msg_type msg_type, va_list args);
static void push_word_ump(struct ump_chan *uc, struct ump_msg *um, int *word_ind, uintptr_t word);
static void send_remaining_ump(struct ump_chan *uc, struct ump_msg *um, int *word_ind);
static errval_t aos_rpc_unmarshall_ump_simple_aarch64(struct aos_rpc *rpc, void *handler, struct aos_rpc_function_binding *binding, struct ump_msg *msg);
static errval_t aos_rpc_call_lmp(struct aos_rpc *rpc, enum aos_rpc_msg_type msg_type, va_list args);
static void push_word_lmp(struct lmp_chan *lc, struct lmp_msg_info *lmi, uintptr_t word);
static uintptr_t pull_word_ump(struct ump_chan *uc, struct ump_msg *um, int *word_ind);
static void push_cap_lmp(struct lmp_chan *lc, struct lmp_msg_info *lmi, struct capref to_push);
static struct capref pull_cap_lmp(struct lmp_chan *lc, struct lmp_msg_info *lmi);
static void send_remaining_lmp(struct lmp_chan *lc, struct lmp_msg_info *lmi);
static errval_t aos_rpc_unmarshall_retval_aarch64(struct aos_rpc *rpc, void **retptrs, struct aos_rpc_function_binding *binding, struct lmp_recv_msg *msg, struct capref cap);
static errval_t aos_rpc_unmarshall_lmp_aarch64(struct aos_rpc *rpc, void *handler, struct aos_rpc_function_binding *binding,
                                               struct lmp_msg_info *lmi);


/* ================== Global RPC Processing ================== */


/**
 * \brief Initializes the RPC channel bindings and sets the default RPC channel bindings.
 */
errval_t aos_rpc_set_interface(struct aos_rpc *rpc, struct aos_rpc_interface *interface, size_t n_handlers, void **handlers)
{
    rpc->interface = interface;
    rpc->n_handlers = n_handlers;
    rpc->handlers = handlers;

    return SYS_ERR_OK;

    /*
    // initiate function binding (to send our endpoint to init)
    aos_rpc_initialize_binding(rpc, AOS_RPC_INITIATE, 1, 0, AOS_RPC_CAPABILITY);

    // bind further functions
    aos_rpc_initialize_binding(rpc, AOS_RPC_SEND_NUMBER, 1, 0, AOS_RPC_WORD);
    aos_rpc_initialize_binding(rpc, AOS_RPC_SEND_STRING, 1, 0, AOS_RPC_VARSTR);
    aos_rpc_initialize_binding(rpc, AOS_RPC_SEND_VARBYTES, 1, 0, AOS_RPC_VARBYTES);
    aos_rpc_initialize_binding(rpc, AOS_RPC_REQUEST_RAM, 2, 2, AOS_RPC_WORD, AOS_RPC_WORD, AOS_RPC_CAPABILITY, AOS_RPC_WORD);

    aos_rpc_initialize_binding(rpc, AOS_RPC_SETUP_PAGE, 3, 0, AOS_RPC_WORD, AOS_RPC_WORD, AOS_RPC_CAPABILITY);
    aos_rpc_register_handler(rpc, AOS_RPC_SETUP_PAGE, &aos_rpc_setup_page_handler);
    aos_rpc_initialize_binding(rpc, AOS_RPC_PROC_SPAWN_REQUEST, 2, 1, AOS_RPC_VARSTR, AOS_RPC_WORD, AOS_RPC_WORD);

    aos_rpc_initialize_binding(rpc, AOS_RPC_FOREIGN_SPAWN, 2, 1, AOS_RPC_VARSTR, AOS_RPC_WORD, AOS_RPC_WORD);

    aos_rpc_initialize_binding(rpc, AOS_RPC_PUTCHAR, 1, 0, AOS_RPC_WORD);
    aos_rpc_initialize_binding(rpc, AOS_RPC_GETCHAR, 0, 1, AOS_RPC_WORD);

    aos_rpc_initialize_binding(rpc, AOS_RPC_ROUNDTRIP, 0, 0);

    aos_rpc_initialize_binding(rpc, AOS_RPC_GET_STDIN_EP, 0, 1, AOS_RPC_CAPABILITY);
    aos_rpc_initialize_binding(rpc, AOS_RPC_SET_STDOUT_EP, 1, 0, AOS_RPC_CAPABILITY);

    // process manager bindings
    aos_rpc_initialize_binding(rpc, AOS_RPC_SERVICE_ON, 1, 0, AOS_RPC_WORD);
    aos_rpc_initialize_binding(rpc, AOS_RPC_REGISTER_PROCESS, 2, 1, AOS_RPC_WORD, AOS_RPC_VARSTR,AOS_RPC_WORD);
    aos_rpc_initialize_binding(rpc, AOS_RPC_GET_PROC_NAME, 1, 1, AOS_RPC_WORD, AOS_RPC_VARSTR);

    aos_rpc_initialize_binding(rpc,AOS_RPC_GET_PROC_LIST,0,2,AOS_RPC_WORD,AOS_RPC_VARSTR);

    aos_rpc_initialize_binding(rpc, AOS_RPC_GET_PROC_CORE,1,1, AOS_RPC_WORD,AOS_RPC_WORD);

    aos_rpc_initialize_binding(rpc, AOS_RPC_BINDING_REQUEST,4,1,AOS_RPC_WORD,AOS_RPC_WORD,AOS_RPC_WORD,AOS_RPC_CAPABILITY,AOS_RPC_CAPABILITY);

    //memory server bindings
    aos_rpc_initialize_binding(rpc,AOS_RPC_MEM_SERVER_REQ,1,1,AOS_RPC_CAPABILITY,AOS_RPC_CAPABILITY);


    //aos_rpc_initialize_binding(rpc, AOS_RPC_REGISTER_PROCESS, 3, 0, AOS_RPC_WORD, AOS_RPC_WORD, AOS_RPC_VARSTR);
    return SYS_ERR_OK;*/
}


/**
 * \brief Initialize an RPC struct over LMP. 
 *
 * \param self_ep local endpoint capability or `NULL_CAP` if no enndpoint is supplied
 * \param end_ep remote endpoint capability
 * \param lmp_ep `lmp_endpoint` struct, will be created if `NULL` is supplied
 */
errval_t aos_rpc_init_lmp(struct aos_rpc* rpc, struct capref self_ep, struct capref end_ep, struct lmp_endpoint *lmp_ep, struct waitset *waitset) {
    errval_t err;

    rpc->backend = AOS_RPC_LMP;

    rpc->lmp_server_mode = false;

    if (waitset) {
        rpc->waitset = waitset;
    }
    else {
        rpc->waitset = get_default_waitset();
    }

    lmp_chan_init(&rpc->channel.lmp);
    rpc->channel.lmp.remote_cap = end_ep;

    const size_t default_buflen = 256;
    if (lmp_ep == NULL) {
        err = endpoint_create(default_buflen, &self_ep, &lmp_ep);
        ON_ERR_PUSH_RETURN(err, LIB_ERR_ENDPOINT_CREATE);
    }


    rpc->channel.lmp.buflen_words = default_buflen;
    rpc->channel.lmp.endpoint = lmp_ep;
    rpc->channel.lmp.local_cap = self_ep;

    err = lmp_chan_alloc_recv_slot(&rpc->channel.lmp);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_LMP_ALLOC_RECV_SLOT);

    err = lmp_chan_register_recv(&rpc->channel.lmp, rpc->waitset, MKCLOSURE(&aos_rpc_on_lmp_message, rpc));
    ON_ERR_PUSH_RETURN(err, LIB_ERR_LMP_ENDPOINT_REGISTER);

    return SYS_ERR_OK;
}


/**
 * \brief Initialize an aos_rpc struct running on UMP backend with default settings
 * Shared page is splitted 1:1 and ump message size is UMP_MSG_SIZE_DEFAULT todo
 *
 * \param first_half Boolean for choosing which part of the shared page is used for sending
 *                   (needs to be inverted on the other end)
 */
errval_t aos_rpc_init_ump_default(struct aos_rpc *rpc, lvaddr_t shared_page, size_t shared_page_size, bool first_half)
{
    errval_t err;

    rpc->backend = AOS_RPC_UMP;

    rpc->waitset = get_default_waitset();

    size_t half_page_size = shared_page_size / 2;

    assert(half_page_size % UMP_MSG_SIZE == 0);
    assert(half_page_size + half_page_size == shared_page_size);

    void *send_pane = (void *) shared_page;
    void *recv_pane = (void *) shared_page + half_page_size;

    if (first_half) {
        // swap panes on one end of the connection
        void *t;
        t = send_pane;
        send_pane = recv_pane;
        recv_pane = t;
    }

    err = ump_chan_init(&rpc->channel.ump, send_pane, half_page_size, recv_pane, half_page_size);
    ON_ERR_RETURN(err);

    // debug_printf("Here!\n");
    err = ump_chan_register_recv(&rpc->channel.ump, rpc->waitset, MKCLOSURE(&aos_rpc_on_ump_message, rpc));
    //err = ump_chan_register_polling(ump_chan_get_default_poller(), &rpc->channel.ump, &aos_rpc_on_ump_message, rpc);
    ON_ERR_RETURN(err);

    return SYS_ERR_OK;
}


errval_t aos_rpc_free(struct aos_rpc *rpc)
{
    if (rpc->backend == AOS_RPC_LMP) {
        lmp_chan_destroy(&rpc->channel.lmp);
    }
    else if (rpc->backend == AOS_RPC_UMP) {
        ump_chan_destroy(&rpc->channel.ump);
    }

    return SYS_ERR_OK;
}

/**
* \brief Initialize marshalling info for an rpc function
*
* In order to use an rpc msg_type, it needs to be registered here.
* Once registered, the function can be called by using \link aos_rpc_call .
*
* Note that the callee domain still needs to set a handler for this function
* using \link aos_rpc_register_handler.
*
* \param msg_type The function id to register
* \param n_args Number of arguments
* \param n_rets Number of return arguments
* \param ... The remaining parameters are of type <code>enum aos_rpc_argument_type</code>
*            first the \link n_args types of the arguments, then the \link n_rets types
*            of the return types.
*/
errval_t aos_rpc_initialize_binding(struct aos_rpc_interface *interface, const char *name, int msg_type, int n_args, int n_rets, ...)
{   

    assert(msg_type < interface->n_bindings);

    struct aos_rpc_function_binding *fb = &interface->bindings[msg_type];
    va_list args;
    fb->n_args = n_args;
    fb->n_rets = n_rets;
    fb->msg_type = msg_type;

    strncpy(fb->binding_name, name, sizeof fb->binding_name);

    va_start(args, n_rets);
    for (int i = 0; i < n_args; i++) {
        fb->args[i] = va_arg(args, int); // read enum values promoted to int
    }
    for (int i = 0; i < n_rets; i++) {
        fb->rets[i] = va_arg(args, int); // read enum values promoted to int
    }
    va_end(args);

    return SYS_ERR_OK;
}

/**
 * \brief Registers a handler function to be called when this rpc is invoked
 *        and should be run in our domain.
 *
 * The handler should take arguments corresponding to the registered binding --
 *
 *        AOS_RPC_WORD          becomes uintptr_t
 *        AOS_RPC_STR           becomes const char*
 *        AOS_RPC_BYTES         currently unimplemented
 *        AOS_RPC_CAPABILITY    becomes struct capref
 *
 * followed by the return values, that are of the corresponding pointer type.
 * They each point to a valid location and need to be written to in order to
 * return any values.
 */
errval_t aos_rpc_register_handler(struct aos_rpc *rpc, enum aos_rpc_msg_type msg_type, void* handler)
{
    assert(msg_type < rpc->n_handlers);
    rpc->handlers[msg_type] = handler;
    return SYS_ERR_OK;
}

/**
 * \brief Abstract function call for every message to be sent to another
 * process on any core
 *
 * \param msg_type The type of message to be sent/funtion to call
 * \param ... Parameters for the message to be sent.
 * Need to be of the types expected by this function.
 *            AOS_RPC_WORD          becomes uintptr_t
 *            AOS_RPC_STR           becomes const char*
 *            AOS_RPC_BYTES         currently unimplemented
 *            AOS_RPC_CAPABILITY    becomes struct capref
 */
errval_t aos_rpc_call(struct aos_rpc *rpc, enum aos_rpc_msg_type msg_type, ...)
{   


    va_list args;
    va_start(args, msg_type);

    errval_t err = 0;
    switch(rpc->backend) {
    case AOS_RPC_UMP:
        err = aos_rpc_call_ump(rpc, msg_type, args);
        break;

    case AOS_RPC_LMP:
        err = aos_rpc_call_lmp(rpc, msg_type, args);
        break;

    }
    va_end(args);

    return err;
}

/**
 * \brief Handler for mapping a newly sent frame into the own virtual address space.
 * Is called for setting up a shared page between to endpoints.
 */
__unused
static void aos_rpc_setup_page_handler(struct aos_rpc* rpc, uintptr_t msg_type, uintptr_t frame_size, struct capref frame)
{
    HERE;
    debug_printf("NYI!\n");
    /*debug_printf("aos_rpc_setup_page_handler\n");
    errval_t err = paging_map_frame(get_current_paging_state(), &rpc->bindings[msg_type].buf_page_remote, frame_size, frame, NULL, NULL);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "setting up communication page\n");
    }*/
}

/**
 * \brief Function for creating the buffer page for sending a (rather long) string over
 * the LMP channel.
 */
static errval_t setup_buf_page(struct aos_rpc *rpc, enum aos_rpc_msg_type msg_type, uintptr_t buf_size)
{
    return LIB_ERR_NOT_IMPLEMENTED;
    /*struct capref frame;
    errval_t err;

    // create frame to share
    err = frame_alloc(&frame, buf_size, NULL);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_FRAME_ALLOC);

    // map into local address space and store address in rpc->bindings[msg_type].buf_page
    err = paging_map_frame_complete(get_current_paging_state(), &rpc->bindings[msg_type].buf_page, frame, NULL, NULL);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_VSPACE_MAP);

    // call remote end to install shared frame into its own address space
    err = aos_rpc_call(rpc, AOS_RPC_SETUP_PAGE, msg_type, buf_size, frame);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_RPC_SETUP_PAGE); // TODO: New error code

    return SYS_ERR_OK;*/
}


/* ===================== UMP ===================== */


/**
 * \brief Abstract function call for every message to be sent to another
 * process on another core
 *
 * \param rpc The RPC channel
 * \param msg_type The type of messege to be sent
 * \param ... Parameters for the message to be sent
 * \return err Error code
 */
static errval_t aos_rpc_call_ump(struct aos_rpc *rpc, enum aos_rpc_msg_type msg_type, va_list args)
{   
    assert(rpc);
    assert(rpc->backend == AOS_RPC_UMP);
    assert(rpc->interface);


    struct aos_rpc_function_binding *binding = &rpc->interface->bindings[msg_type];
    size_t n_args = binding->n_args;
    size_t n_rets = binding->n_rets;
    void* retptrs[4];

    /* struct ump_msg um = DECLARE_MESSAGE(rpc->channel.ump); */
    DECLARE_MESSAGE(rpc->channel.ump, um);
    um->flag = 0;
    um->data[0] = msg_type;

    // Send
    int word_ind = 1;
    int ret_ind = 0;
    for (int i = 0; i < n_args; i++) {
        if (binding->args[i] == AOS_RPC_WORD) {
            push_word_ump(&rpc->channel.ump, um, &word_ind, va_arg(args, uintptr_t));
        }
        else if (binding->args[i] == AOS_RPC_SHORTSTR) {
            const int n_words = AOS_RPC_SHORTSTR_LENGTH / sizeof(uintptr_t);
            uintptr_t words[n_words];

            const char *str = va_arg(args, char*);
            assert(strlen(str) < AOS_RPC_SHORTSTR_LENGTH);

            memcpy(&words, str, strlen(str));
            for (int j = 0; j < n_words; j++) {
                push_word_ump(&rpc->channel.ump, um, &word_ind, words[j]);
            }
        }
        else if (binding->args[i] == AOS_RPC_CAPABILITY) {
            struct capref cr = va_arg(args, struct capref);
            struct capability cap;
            // non-portable assertion
            static_assert(sizeof(struct capability) == 3 * sizeof(uintptr_t));
            uintptr_t words[3];
            invoke_cap_identify(cr, &cap);
            memcpy(&words, &cap, sizeof(struct capability));
            for (int j = 0; j < 3; j++) {
                push_word_ump(&rpc->channel.ump, um, &word_ind, words[j]);
            }
        }
        else if (binding->args[i] == AOS_RPC_VARSTR) {
            const char *str = va_arg(args, char*);
            size_t msg_len = strlen(str) + 1;
            push_word_ump(&rpc->channel.ump, um, &word_ind, msg_len);
            for (int j = 0; j < msg_len; j += sizeof(uintptr_t)) {
                int word_len = min(sizeof(uintptr_t), msg_len - j);
                uintptr_t word = 0;
                memcpy(&word, str + j, word_len);
                push_word_ump(&rpc->channel.ump, um, &word_ind, word);
            }
        }
        else if (binding->args[i] == AOS_RPC_VARBYTES) {
            struct aos_rpc_varbytes bytes = va_arg(args, struct aos_rpc_varbytes);
            uintptr_t len = bytes.length;
            push_word_ump(&rpc->channel.ump, um, &word_ind, len);
            for (int j = 0; j < len; j += sizeof(uintptr_t)) {
                int word_len = min(sizeof(uintptr_t), len - j);
                uintptr_t word = 0;
                memcpy(&word, bytes.bytes + j, word_len);
                push_word_ump(&rpc->channel.ump, um, &word_ind, word);
            }
        }
        else {
            debug_printf("message type over ump NYI!\n");
            return LIB_ERR_NOT_IMPLEMENTED;
        }
    }

    send_remaining_ump(&rpc->channel.ump, um, &word_ind);

    // Receive
    for (int i = 0; i < n_rets; i++) {
        retptrs[ret_ind++] = va_arg(args, void*);
    }
    DECLARE_MESSAGE(rpc->channel.ump, response);
    bool received = false;
    do {
        received = ump_chan_poll_once(&rpc->channel.ump, response);
    } while (!received);

    if (!((response->data[0] | AOS_RPC_RETURN_BIT)
          && (response->data[0] & ~AOS_RPC_RETURN_BIT) == msg_type)) {
        return LIB_ERR_NOT_IMPLEMENTED;  // todo errcode
    }

    /*debug_printf("response words: %ld %ld %ld %ld %ld %ld %ld\n",
        response->data[0],
        response->data[1],
        response->data[2],
        response->data[3],
        response->data[4],
        response->data[5],
        response->data[6]
    );*/

    int ret_offs = 1;
    for (int i = 0; i < n_rets; i++) {
        switch(binding->rets[i]) {
        case AOS_RPC_WORD: {
            *((uintptr_t *) retptrs[i]) = pull_word_ump(&rpc->channel.ump, response, &ret_offs);
        }
        break;
        case AOS_RPC_CAPABILITY: {
            uintptr_t vals[3];
            vals[0] = pull_word_ump(&rpc->channel.ump, response, &ret_offs);
            vals[1] = pull_word_ump(&rpc->channel.ump, response, &ret_offs);
            vals[2] = pull_word_ump(&rpc->channel.ump, response, &ret_offs);

            struct capability cap;
            
            memcpy(&cap, &vals, sizeof cap);
            ret_offs += sizeof(struct capability) / sizeof(uintptr_t);

            struct capref forged;
            errval_t err = slot_alloc(&forged);
            ON_ERR_PUSH_RETURN(err, LIB_ERR_SLOT_ALLOC);

            //char buffer[512];
            //debug_print_cap(buffer,512,&cap);
            //debug_printf("cap to forge: %s\n",buffer);


            err = invoke_monitor_create_cap((uint64_t *) &cap,
                                            get_cnode_addr(forged),
                                            get_cnode_level(forged),
                                            forged.slot,
                                            disp_get_core_id()); // TODO: set owner correctly
            if (err_is_fail(err)) {
                DEBUG_ERR(err, "forging of cap failed\n");
            }
            ON_ERR_PUSH_RETURN(err, LIB_ERR_MONITOR_CAP_SEND);
            *((struct capref *) retptrs[i]) = forged;
        }
        break;
        case AOS_RPC_VARSTR: {
            char *ret = (char *) retptrs[i];
            size_t len = pull_word_ump(&rpc->channel.ump, response, &ret_offs);

            for (size_t j = 0; j < len; j += 8) {
                uintptr_t word = pull_word_ump(&rpc->channel.ump, response, &ret_offs);
                int word_len = min(sizeof(uintptr_t), len - j);
                memcpy(ret + j, &word, word_len);
            }
        }
        break;
        case AOS_RPC_VARBYTES: {
            size_t len = pull_word_ump(&rpc->channel.ump, response, &ret_offs);

            struct aos_rpc_varbytes *ret = (struct aos_rpc_varbytes *) retptrs[i];
            for (size_t j = 0; j < len; j += 8) {
                uintptr_t word = pull_word_ump(&rpc->channel.ump, response, &ret_offs);
                int word_len = min(sizeof(uintptr_t), len - j);
                memcpy(ret + j, &word, word_len);
            }
        }
        break;
        default:
            debug_printf("UNHANDLED ARGUMENT TYPE NYI\n");
        break;
        }
    }

    return SYS_ERR_OK;
}

/**
 * \brief Message handler function for rpc calls via ump
 */
void aos_rpc_on_ump_message(void *arg)
{
    // debug_printf("ump message received!\n");
    errval_t err;

    struct aos_rpc *rpc = arg;
    DECLARE_MESSAGE(rpc->channel.ump, msg);
    msg->flag = 0;

    bool received = ump_chan_poll_once(&rpc->channel.ump, msg);
    if (!received) {
        //debug_printf("aos_rpc_on_ump_message called but no message available\n");
        ump_chan_register_recv(&rpc->channel.ump, rpc->waitset, MKCLOSURE(&aos_rpc_on_ump_message, rpc));
        return;
    }

    enum aos_rpc_msg_type msgtype = msg->data[0];

    void *handler = rpc->handlers[msgtype];
    if (handler == NULL) {
        debug_printf("no handler for %d\n", msgtype);
        ump_chan_register_recv(&rpc->channel.ump, rpc->waitset, MKCLOSURE(&aos_rpc_on_ump_message, rpc));
        return;
    }

    struct aos_rpc_function_binding *binding = &rpc->interface->bindings[msgtype];

    err = aos_rpc_unmarshall_ump_simple_aarch64(rpc, handler, binding, msg);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "error in unmarshall\n");
    }

    ump_chan_register_recv(&rpc->channel.ump, rpc->waitset, MKCLOSURE(&aos_rpc_on_ump_message, rpc));

    // in case of simple binding
    /*if (binding->calling_simple) {
        err = aos_rpc_unmarshall_ump_simple_aarch64(rpc, handler, binding, msg);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "error in unmarshall\n");
        }
    }
    else {
        debug_printf("NYI in aos_rpc_on_ump_message\n");
    }*/
}

/**
 * \brief Helper function for retrieving a word of a ump message

 * \param um Address to UMP message array
 * \param word_ind Address to index in to the message array
 * \return word Retrieved message
 */
static uintptr_t pull_word_ump(struct ump_chan *uc, struct ump_msg *um, int *word_ind)
{
    if (*word_ind >= UMP_MSG_N_WORDS) {
        bool received = false;
        do {
            received = ump_chan_poll_once(uc, um);
        } while(!received);
        *word_ind = 0;
    }
    uintptr_t word = um->data[*word_ind];
    (*word_ind)++;

    return word;
}

/**
 * \brief UMP helper function for adding word snips to the message array and sending the array
 * over the channel if full
 *
 * \param uc UMP channel
 * \param um UMP message array
 * \param word_ind Index into the message array
 * \param word Pointer to 8 byte message
 */
static void push_word_ump(struct ump_chan *uc, struct ump_msg *um, int *word_ind, uintptr_t word)
{
    um->data[(*word_ind)++] = word;

    if (*word_ind >= UMP_MSG_N_WORDS) {
        bool sent = false;
        do {
            sent = ump_chan_send(uc, um);
        } while (!sent);
        *word_ind = 0;
    }
}

/**
 * \brief UMP helper function for sending the remaining message snips
 * \param uc UMP channel
 * \param um UMP message array
 * \param word_ind Index into the message array
 */
static void send_remaining_ump(struct ump_chan *uc, struct ump_msg *um, int *word_ind)
{
    if (*word_ind > 0) {
        bool sent = false;
        do {
            sent = ump_chan_send(uc, um);
        } while (!sent);
        *word_ind = 0;
    }
}

static errval_t aos_rpc_unmarshall_ump_simple_aarch64(struct aos_rpc *rpc, void *handler, struct aos_rpc_function_binding *binding,
                                                      struct ump_msg *msg)
{
    // debug_printf("words: %ld %ld %ld %ld %ld %ld %ld\n",
    //     msg->data[0], msg->data[1], msg->data[2], msg->data[3],
    //     msg->data[4], msg->data[5], msg->data[6]
    // );

    

    struct ump_chan *uc = &rpc->channel.ump;

    typedef uintptr_t ui;
    ui arg[8] = { 0 }; // Upper bound so only writes in register
    ui stack_args[16] = { 0 };
    ui ret[8] = { 0 };
    errval_t (*hd)(struct aos_rpc*, ui, ui, ui, ui, ui, ui, ui,
                                ui, ui, ui, ui, ui, ui, ui, ui,
                                ui, ui, ui, ui, ui, ui, ui, ui) = handler;
    // index into msg
    int word_ind = 1;

    int a_pos = 0;
    int a_stack_pos = 0;
    int ret_pos = 0;
    struct capref retcap;
    char argstring[4096]; // very dangerous
    char retstring[4096]; // very dangerous

    char abytes[4096]; // very dangerous
    char rbytes[4096]; // very dangerous
    struct aos_rpc_varbytes argbytes = { .length = sizeof abytes, .bytes = abytes };
    struct aos_rpc_varbytes retbytes = { .length = sizeof rbytes, .bytes = rbytes };

    void argword(ui aw) {
        if (a_pos < 7 && a_stack_pos == 0) {
            arg[a_pos++] = aw;
        }
        else {
            stack_args[a_stack_pos++] = aw;
        }
    }
    void argdoubleword(ui x1, ui x2) {
        if (a_pos < 6 && a_stack_pos == 0) {
            arg[a_pos++] = x1;
            arg[a_pos++] = x2;
        }
        else {
            stack_args[a_stack_pos++] = x1;
            stack_args[a_stack_pos++] = x2;
        }
    }


    for (int i = 0; i < binding->n_args; i++) {

        switch(binding->args[i]) {
        case AOS_RPC_WORD: {
            argword(pull_word_ump(uc, msg, &word_ind));
        }
        break;
        case AOS_RPC_CAPABILITY: {
            uintptr_t words[3];
            words[0] = pull_word_ump(uc, msg, &word_ind);
            words[1] = pull_word_ump(uc, msg, &word_ind);
            words[2] = pull_word_ump(uc, msg, &word_ind);

            struct capability cap;
            memcpy(&cap, words, sizeof cap);

            struct capref forged;
            errval_t err = slot_alloc(&forged);
            ON_ERR_PUSH_RETURN(err, LIB_ERR_SLOT_ALLOC);


            // debug_printf("Here is forged cap:\n");
            // char buf[512];
            // debug_print_cap(buf,512,&cap);
            // debug_printf("%s\n",buf);
            
            err = invoke_monitor_create_cap((uint64_t *) &cap,
                                            get_cnode_addr(forged),
                                            get_cnode_level(forged),
                                            forged.slot, !disp_get_core_id()); // TODO: set owner correctly
            ON_ERR_PUSH_RETURN(err, LIB_ERR_MONITOR_CAP_SEND);

            uintptr_t x1 = (forged.cnode.croot) | (((ui) forged.cnode.cnode) << 32);
            uintptr_t x2 = (forged.cnode.level) | (((ui) forged.slot) << 32);
            argdoubleword(x1, x2);

        }
        break;
        case AOS_RPC_VARSTR: {
            uintptr_t length = pull_word_ump(uc, msg, &word_ind);
            assert(length < sizeof argstring);
            for (size_t j = 0; j < length; j += sizeof(uintptr_t)) {
                uintptr_t piece = pull_word_ump(uc, msg, &word_ind);
                memcpy(argstring + j, &piece, min(sizeof(uintptr_t), length - j));
            }
            argword((ui) &argstring);
        }
        break;
        case AOS_RPC_VARBYTES: {
            uintptr_t length = pull_word_ump(uc, msg, &word_ind);
            assert(length < sizeof abytes);
            argbytes.length = length;
            for (size_t j = 0; j < length; j += sizeof(uintptr_t)) {
                uintptr_t piece = pull_word_ump(uc, msg, &word_ind);
                memcpy(argbytes.bytes + j, &piece, min(sizeof(uintptr_t), length - j));
            }
            uintptr_t av[2];
            memcpy(av, &argbytes, sizeof argbytes);
            argdoubleword(av[0], av[1]);
        }
        break;
        default:
            debug_printf("only integer and shortstr arguments supported at the moment\n");
            return LIB_ERR_NOT_IMPLEMENTED;
            break;
        }
    }

    bool retcap_used = false;
    for (int i = 0; i < binding->n_rets; i++) {

        switch(binding->rets[i]) {
        case AOS_RPC_WORD: {
            argword((ui) &ret[ret_pos++]);
        }
        break;
        case AOS_RPC_SHORTSTR: {
            argword((ui) &ret[ret_pos]);
            ret_pos += 4;
        }
        break;
        case AOS_RPC_CAPABILITY: {
            if (retcap_used) return LIB_ERR_NOT_IMPLEMENTED; // error, sending multiple caps nyi
            argword((ui) &retcap);
            retcap_used = true;
        }
        break;
        case AOS_RPC_STR: {
            argword((ui) &retstring); // todo replace with scratch space
        }
        break;
        case AOS_RPC_VARSTR: {
            argword((ui) &retstring); // todo replace with scratch space
        }
        break;
        case AOS_RPC_VARBYTES: {
            argword((ui) &retbytes); // todo replace with scratch space
        }
        break;

        default:
            debug_printf("unhandled ret arg\n");
            break;
        }

        if (a_pos > 7) {
            return LIB_ERR_SLAB_ALLOC_FAIL; // todo
        }
    }

    hd(rpc, arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6],
       stack_args[0], stack_args[1], stack_args[2], stack_args[3],
       stack_args[4], stack_args[5], stack_args[6], stack_args[7],
       stack_args[8], stack_args[9], stack_args[10], stack_args[11],
       stack_args[12], stack_args[13], stack_args[14], stack_args[15]);
    

    // send response
    DECLARE_MESSAGE(rpc->channel.ump, response);
    response->flag = 0;
    response->data[0] = binding->msg_type | AOS_RPC_RETURN_BIT;

    int buf_pos = 1;
    ret_pos = 0;
    for (int i = 0; i < binding->n_rets; i++) {
        switch(binding->rets[i]) {
        case AOS_RPC_WORD: {
            uintptr_t word = ret[ret_pos++];
            push_word_ump(uc, response, &buf_pos, word);
        }
        break;

        case AOS_RPC_CAPABILITY: {
            struct capability cap;
            errval_t err = invoke_cap_identify(retcap, &cap);
            ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_IDENTIFY);
            uintptr_t words[3];
            memcpy(&words, &cap, sizeof cap);
            push_word_ump(uc, response, &buf_pos, words[0]);
            push_word_ump(uc, response, &buf_pos, words[1]);
            push_word_ump(uc, response, &buf_pos, words[2]);
        }
        break;

        case AOS_RPC_VARSTR: {
            uintptr_t length = strlen(retstring);
            push_word_ump(uc, response, &buf_pos, length);

            for (int j = 0; j < length; j += sizeof(uintptr_t)) {
                uintptr_t word;
                memcpy(&word, retstring + j, min(sizeof(uintptr_t), length - j));
                push_word_ump(uc, response, &buf_pos, word);
            }
        }
        break;

        case AOS_RPC_VARBYTES: {
            uintptr_t length = retbytes.length;
            push_word_ump(uc, response, &buf_pos, length);

            for (int j = 0; j < length; j += sizeof(uintptr_t)) {
                uintptr_t word;
                memcpy(&word, retbytes.bytes + j, min(sizeof(uintptr_t), length - j));
                push_word_ump(uc, response, &buf_pos, word);
            }
        }
        break;

        default:
            debug_printf("unhandled ret arg\n");
            break;
        }
    }

    send_remaining_ump(uc, response, &buf_pos);

    return SYS_ERR_OK;
}


/* ===================== LMP ===================== */



/**
 * \brief Abstract function call for every message to be sent to another
 * process on the same core
 *
 * \param rpc The RPC channel
 * \param msg_type The type of messege to be sent
 * \param args Parameters for the message to be sent
 * \return err Error code
 */
static errval_t aos_rpc_call_lmp(struct aos_rpc *rpc, enum aos_rpc_msg_type msg_type, va_list args)
{




    // debug_printf("THis domain: %d is calling call with type %d\n",disp_get_domain_id(),msg_type );

    assert(rpc);
    assert(rpc->backend == AOS_RPC_LMP);
    assert(rpc->interface);

    errval_t err;

    struct lmp_chan *lc = &rpc->channel.lmp;



    struct aos_rpc_function_binding *binding = &rpc->interface->bindings[msg_type];
    size_t n_args = binding->n_args;
    size_t n_rets = binding->n_rets;
    void* retptrs[8];

    struct lmp_msg_info lmi;
    lmi.cap = NULL_CAP;
    lmi.cap_taken = false;
    size_t buf_page_offset = 0;


    if (rpc->lmp_server_mode) {
        push_cap_lmp(lc, &lmi, rpc->channel.lmp.local_cap);
    }

    lmi.msg.words[0] = msg_type;
    lmi.word_index = 1;
    int ret_ind = 0;
    for (int i = 0; i < n_args; i++) {
        switch(binding->args[i]) {
        case AOS_RPC_WORD: {
            uintptr_t word = va_arg(args, uintptr_t);
            push_word_lmp(lc, &lmi, word);
        }
        break;
        case AOS_RPC_CAPABILITY: {
            struct capref cap = va_arg(args, struct capref);
            push_cap_lmp(lc, &lmi, cap);
        }
        break;
        case AOS_RPC_VARSTR: {
            const char *str = va_arg(args, const char *);
            uintptr_t length = strlen(str) + 1;
            push_word_lmp(lc, &lmi, length);

            for (int j = 0; j < length; j += sizeof(uintptr_t)) {
                uintptr_t word = 0;
                memcpy(&word, str + j, min(sizeof(uintptr_t), length - j));
                push_word_lmp(lc, &lmi, word);
            }
        }
        break;
        case AOS_RPC_VARBYTES: {
            struct aos_rpc_varbytes bytes = va_arg(args, struct aos_rpc_varbytes);
            uintptr_t len = bytes.length;
            push_word_lmp(lc, &lmi, len);

            for (int j = 0; j < len; j += sizeof(uintptr_t)) {
                uintptr_t word = 0;
                memcpy(&word, bytes.bytes + j, min(sizeof(uintptr_t), len - j));
                push_word_lmp(lc, &lmi, word);
            }
        }
        break;
        case AOS_RPC_STR: {
            const char* str = va_arg(args, const char*);
            size_t buf_page_size = BASE_PAGE_SIZE * 4;

            if (binding->buf_page == NULL) {
                if (strlen(str) > buf_page_size) {
                    debug_printf("String is too big to send");
                    return LIB_ERR_STRING_TOO_LONG;
                }
                err = setup_buf_page(rpc, msg_type, buf_page_size);
                ON_ERR_PUSH_RETURN(err, LIB_ERR_FRAME_ALLOC); // Todo: create new error code
            }
            // send the offset into the buf_page
            push_word_lmp(lc, &lmi, buf_page_offset);
            size_t length = strlen(str);
            if (length + buf_page_offset >= BASE_PAGE_SIZE * 4) length = BASE_PAGE_SIZE * 4 - buf_page_offset;
            strncpy(binding->buf_page + buf_page_offset, str, length);
            buf_page_offset += length;
        }
        break;

        default:
            debug_printf("unknown arg type\n");
            break;
        }
    }

    // debug_printf("THis domain: %d is calling call with type %d\n",disp_get_domain_id(),msg_type );

    send_remaining_lmp(lc, &lmi);

    for (int i = 0; i < n_rets; i++) {
        retptrs[ret_ind++] = va_arg(args, void*);
    }

    //debug_printf("waiting for response\n");
    while(!lmp_chan_can_recv(&rpc->channel.lmp)) {
        // yield to dispatcher we are communicating with
        // debug_printf("waiting for response\n");
        thread_yield_dispatcher(rpc->channel.lmp.remote_cap);
    }

    //debug_printf("getting response\n");

    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref recieved_cap = NULL_CAP;

    err = lmp_chan_recv(&rpc->channel.lmp, &msg, &recieved_cap);
    ON_ERR_RETURN(err);
    //debug_printf("got response\n");

    if (!capref_is_null(recieved_cap)) {
        lmp_chan_alloc_recv_slot(&rpc->channel.lmp);
    }

    err = aos_rpc_unmarshall_retval_aarch64(rpc, retptrs, binding, &msg, recieved_cap);
    ON_ERR_RETURN(err);

    return SYS_ERR_OK;
}

/**
 * \brief Message handler function for rpc calls via lmp
 */
void aos_rpc_on_lmp_message(void *arg)
{
    //debug_printf("aos_rpc_on_lmp_message\n");
    struct aos_rpc *rpc = arg;
    // debug_printf("PM channel : %lx", get_pm_rpc());
    // debug_printf("Receive channel : %lx",rpc);

    struct lmp_chan *channel = &rpc->channel.lmp;

    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref recieved_cap = NULL_CAP;
    errval_t err;

    err = lmp_chan_recv(channel, &msg, &recieved_cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        debug_printf("transient error\n");
        err = lmp_chan_register_recv(channel, rpc->waitset ? : get_default_waitset(), MKCLOSURE(&aos_rpc_on_lmp_message, arg));
        return;
    }
    else if (err == LIB_ERR_NO_LMP_MSG) {
        err = lmp_chan_register_recv(channel, rpc->waitset ? : get_default_waitset(), MKCLOSURE(&aos_rpc_on_lmp_message, arg));
        return;
    }
    else if (err_is_fail(err)) {
        debug_printf("error is: %ld\n", err);
        err = err_push(err, LIB_ERR_LMP_CHAN_RECV);
        goto on_error;
    }

    if (!capref_is_null(recieved_cap)) {
        //debug_printf("reslotting\n");
        lmp_chan_alloc_recv_slot(channel);
    }

    uintptr_t msgtype = msg.words[0];

    bool is_response = false;
    if (msgtype & AOS_RPC_RETURN_BIT) {
        msgtype &= ~AOS_RPC_RETURN_BIT;
        is_response = true;
        debug_printf("no handler for 0x%lx\n", msgtype);
        err = LIB_ERR_RPC_NO_HANDLER_SET;
        goto on_error;
    }

    if (!rpc->handlers || !rpc->handlers[msgtype] || msgtype > rpc->n_handlers) {
        debug_printf("no handler for %lu\n", msgtype);
        if (rpc->interface->n_bindings > msgtype) {
            debug_printf("for function %s\n", rpc->interface->bindings[msgtype].binding_name);
        }
        err = LIB_ERR_RPC_NO_HANDLER_SET;
        goto on_error;
    }
    void *handler = rpc->handlers[msgtype];

    struct aos_rpc_function_binding *binding = &rpc->interface->bindings[msgtype];

    struct lmp_msg_info lmi;
    lmi.msg = msg;
    lmi.word_index = 1;
    lmi.cap = recieved_cap;
    lmi.cap_taken = false;

    if (rpc->lmp_server_mode) {
        struct capref response_dest = pull_cap_lmp(&rpc->channel.lmp, &lmi);
        struct capability epcap;
        invoke_cap_identify(response_dest, &epcap);
        if (epcap.type != ObjType_EndPointLMP) {
            debug_printf("non-endpoint-type cap received\n");
        }
        else {
            if (!capref_is_null(rpc->channel.lmp.remote_cap)) {
                cap_destroy(rpc->channel.lmp.remote_cap);
            }
            rpc->channel.lmp.remote_cap = response_dest;
            //char buf[128];
            //debug_print_capref(buf, 128, rpc->channel.lmp.remote_cap);
            //debug_printf("setting epcap to %s\n", buf);
            recieved_cap = NULL_CAP;
        }
    }

    err = aos_rpc_unmarshall_lmp_aarch64(rpc, handler, binding, &lmi);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "error unmarshaling lmp message\n");
    }

//on_success:
    //debug_printf("reregister\n");
    err = lmp_chan_register_recv(channel, rpc->waitset ? : get_default_waitset(), MKCLOSURE(&aos_rpc_on_lmp_message, arg));
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "error lmp_chan_register_recv\n");
    }

    /*struct capability recv_cap;
    err = invoke_cap_identify(rpc->channel.lmp.endpoint->recv_slot, &recv_cap);
    if (!err_is_fail(err)) {
        err = lmp_chan_alloc_recv_slot(&rpc->channel.lmp);
        DEBUG_ERR(err, "allocating recv slot\n");
        char buf[128];
        debug_print_cap_at_capref(buf, 128, rpc->channel.lmp.endpoint->recv_slot);
        debug_printf("recvcap is %s\n", buf);
    }*/


    return;

    errval_t err2;
on_error:
    err2 = lmp_chan_register_recv(channel, rpc->waitset ? : get_default_waitset(), MKCLOSURE(&aos_rpc_on_lmp_message, arg));
    DEBUG_ERR(err, "error handling message\n");
    return;
}


static void lmi_receive(struct lmp_chan *lc, struct lmp_msg_info *lmi)
{
    bool can_receive = lmp_chan_can_recv(lc);
    bool received = false;

    while(!can_receive) {
        thread_yield_dispatcher(lc->remote_cap);
        can_receive = lmp_chan_can_recv(lc);
    }

    do {
        lmi->msg = LMP_RECV_MSG_INIT;
        errval_t err = lmp_chan_recv(lc, &lmi->msg, &lmi->cap);
        received = err == SYS_ERR_OK;
    } while(!received);

    lmi->word_index = 0;
    lmi->cap_taken = false;

    if (!capref_is_null(lmi->cap)) {
        // debug_printf("realloccing slot\n");
        lmp_chan_alloc_recv_slot(lc);
    }
}

static void lmi_send(struct lmp_chan *lc, struct lmp_msg_info *lmi)
{
    bool sent = false;
    do {
        errval_t err = lmp_chan_send(lc, LMP_FLAG_YIELD, lmi->cap, 4,
                                        lmi->msg.words[0], lmi->msg.words[1], lmi->msg.words[2], lmi->msg.words[3]);
        sent = err == SYS_ERR_OK;
    } while (!sent);
    lmi->cap = NULL_CAP;
    lmi->cap_taken = false;
    lmi->word_index = 0;
}

/**
 * \brief LMP helper function for retrieving word snips from a already received message
 * array. If the array has already be read, receive a new array.
 *
 * \param lm Address to LMP message array
 * \param cap Address to capability slot of the message
 * \param word_ind Address to index into the message array
 */
static uintptr_t pull_word_lmp(struct lmp_chan *lc, struct lmp_msg_info *lmi)
{
    if (lmi->word_index >= LMP_MSG_LENGTH) {
        lmi_receive(lc, lmi);
    }

    uintptr_t word = lmi->msg.words[lmi->word_index];
    lmi->word_index++;

    return word;
}

/**
 * \brief LMP helper function for retrieving received capability over the channel.
 * If the capref pointer is NULL, we try to receive a new message with a capref in it.
 *
 * \param lm Address to LMP message array
 * \param cap Address to capability slot of the message
 * \param word_ind Address to index into the message array
 */
static struct capref pull_cap_lmp(struct lmp_chan *lc, struct lmp_msg_info *lmi)
{
    if (lmi->cap_taken) {
        lmi_receive(lc, lmi);
    }
    struct capref ret = lmi->cap;
    lmi->cap_taken = true;
    return ret;
}

/**
 * \brief LMP helper function for adding word snips to the message array and sending the array
 * over the channel if full
 *
 * \param uc LMP channel
 * \param um LMP message array
 * \param cap Slot for sending a capability, can be NULL if no cap is to be sent
 * \param word_ind Index into the message array
 * \param word Pointer to 8 byte message
 */
static void push_word_lmp(struct lmp_chan *lc, struct lmp_msg_info *lmi, uintptr_t word)
{
    if (lmi->word_index >= LMP_MSG_LENGTH) {
        lmi_send(lc, lmi);
    }
    lmi->msg.words[lmi->word_index++] = word;
}

/**
 * \brief LMP helper function for adding a capability to the message array and sending the array
 * over the channel if slot already occupied
 *
 * \param uc Address to LMP channel
 * \param um Address to LMP message array
 * \param cap Address to slot for sending a capability, can be NULL if no cap is to be sent
 * \param word_ind Address to index into the message array
 * \param to_push Capability to be added to the message
 */
static void push_cap_lmp(struct lmp_chan *lc, struct lmp_msg_info *lmi, struct capref to_push)
{
    if (lmi->cap_taken) {
        lmi_send(lc, lmi);
    }
    lmi->cap = to_push;
    lmi->cap_taken = true;
}


static void send_remaining_lmp(struct lmp_chan *lc, struct lmp_msg_info *lmi)
{
    if (lmi->word_index > 0 || lmi->cap_taken) {
        bool sent = false;
        do {
            errval_t err = lmp_chan_send(lc, LMP_SEND_FLAGS_DEFAULT, lmi->cap, 4,
                                         lmi->msg.words[0], lmi->msg.words[1], lmi->msg.words[2], lmi->msg.words[3]);

            if (err_is_fail(err) && !lmp_err_is_transient(err)) {
                DEBUG_ERR(err, "what is this?\n");
                break;
            }
            else if (err_is_fail(err)) {
                //err_print_calltrace(err);
                // DEBUG_ERR(err, "interesting\n");
                thread_yield_dispatcher(lc->remote_cap);
            }
        // debug_printf("Stuck here!\n");
            sent = err == SYS_ERR_OK;
        } while (!sent);
        lmi->cap_taken = false;
        lmi->cap = NULL_CAP;
        lmi->word_index = 0;
    }
}

/**
 * \brief Helper function for unmarshalling received message
 *
 * \param retptrs Array of pointers where the return values have to be saved
 * \param binding Index to array of info what argument and return types to unmarshall
 * \param msg Received LMP message
 * \param cap Capability to ??? TODO
 *
 * retvals for calls
 */
static errval_t aos_rpc_unmarshall_retval_aarch64(struct aos_rpc *rpc, void **retptrs, struct aos_rpc_function_binding *binding,
                                                      struct lmp_recv_msg *msg, struct capref cap)
{

    struct lmp_chan *lc = &rpc->channel.lmp;

    struct lmp_msg_info lmi;

    lmi.msg = *msg;
    lmi.cap = cap;
    lmi.cap_taken = false;
    lmi.word_index = 1;

    for (int i = 0; i < binding->n_rets; i++) {
        switch (binding->rets[i]) {
        case AOS_RPC_WORD: {
            *((uintptr_t *) retptrs[i]) = pull_word_lmp(lc, &lmi);
        }
        break;

        case AOS_RPC_CAPABILITY: {
            *((struct capref *) retptrs[i]) = pull_cap_lmp(lc, &lmi);
        }
        break;

        case AOS_RPC_VARSTR: {
            size_t length = pull_word_lmp(lc, &lmi);
            char *str = (char *) retptrs[i];
            for (size_t j = 0; j < length; j += sizeof(uintptr_t)) {
                uintptr_t word = pull_word_lmp(lc, &lmi);
                memcpy(str + j, &word, min(sizeof(uintptr_t), length - j));
            }
        }
        break;
        case AOS_RPC_VARBYTES: {
            size_t length = pull_word_lmp(lc, &lmi);
            struct aos_rpc_varbytes *bytes = (struct aos_rpc_varbytes *) retptrs[i];

            for (int j = 0; j < length; j += sizeof(uintptr_t)) {
                uintptr_t word = pull_word_lmp(lc, &lmi);
                memcpy(bytes->bytes + j, &word, min(sizeof(uintptr_t), length - j));
            }
        }

        default:
            debug_printf("unknown return type in unmarshalling\n");
            break;
        }
    }

    // debug_printf("Got here\n");
    return SYS_ERR_OK;
}
/**
 *
 */
static errval_t aos_rpc_unmarshall_lmp_aarch64(struct aos_rpc *rpc, void *handler, struct aos_rpc_function_binding *binding,
                                               struct lmp_msg_info *lmi)
{
    //debug_printf("words: %ld %ld %ld %ld\n", lmi->msg.words[0], lmi->msg.words[1], lmi->msg.words[2], lmi->msg.words[3]);
    //debug_printf("rpc = %p\n", rpc);
    struct lmp_chan *lc = &rpc->channel.lmp;
    
    typedef uintptr_t ui;
    ui arg[7] = { 0 }; // Upper bound so only writes in register
    ui stack_args[16] = { 0 };

    ui ret[4] = { 0 };
    
    char argstring[1024]; // TODO maybe move that into some scratch area in the rpc struct
    char retstring[1024];

    char abytes[1024];
    struct aos_rpc_varbytes argbytes = { .length = sizeof abytes, .bytes = abytes};
    char rbytes[1024];
    struct aos_rpc_varbytes retbytes = { .length = sizeof rbytes, .bytes = rbytes};
    

    struct capref retcap = NULL_CAP;
    errval_t (*hd)(struct aos_rpc*, ui, ui, ui, ui, ui, ui, ui,
                                ui, ui, ui, ui, ui, ui, ui, ui,
                                ui, ui, ui, ui, ui, ui, ui, ui) = handler;
    int a_pos = 0;
    int a_stack_pos = 0;
    int ret_pos = 0;

    void argword(ui aw) {
        if (a_pos < 7 && a_stack_pos == 0) {
            arg[a_pos++] = aw;
        }
        else {
            stack_args[a_stack_pos++] = aw;
        }
    }
    void argdoubleword(ui x1, ui x2) {
        if (a_pos < 6 && a_stack_pos == 0) {
            arg[a_pos++] = x1;
            arg[a_pos++] = x2;
        }
        else {
            stack_args[a_stack_pos++] = x1;
            stack_args[a_stack_pos++] = x2;
        }
    }
    
    for (int i = 0; i < binding->n_args; i++) {
        switch (binding->args[i]) {
        
        case AOS_RPC_WORD: {
            argword(pull_word_lmp(lc, lmi));
        }
        break;

        case AOS_RPC_CAPABILITY: {
            struct capref recv_cap = pull_cap_lmp(lc, lmi);
            uintptr_t x1 = ((ui) recv_cap.cnode.croot) | (((ui) recv_cap.cnode.cnode) << 32);
            uintptr_t x2 = ((ui) recv_cap.cnode.level) | (((ui) recv_cap.slot) << 32);
            argdoubleword(x1, x2);
        }
        break;

        case AOS_RPC_STR: {
            argword(((ui) binding->buf_page_remote) + pull_word_lmp(lc, lmi));
        }
        break;

        case AOS_RPC_VARSTR: {
            size_t length = pull_word_lmp(lc, lmi);
            // debug_printf("reading str arg %ld\n", length);
            assert(length < sizeof argstring);
            for (size_t j = 0; j < length; j += sizeof(uintptr_t)) {
                uintptr_t piece = pull_word_lmp(lc, lmi);
                memcpy(argstring + j, &piece, sizeof(uintptr_t));
            }
            argword((ui) &argstring);
        }
        break;
        case AOS_RPC_VARBYTES: {
            size_t length = pull_word_lmp(lc, lmi);
            assert(length < sizeof abytes);
            argbytes.length = length;
            for (size_t j = 0; j < length; j += sizeof(uintptr_t)) {
                uintptr_t piece = pull_word_lmp(lc, lmi);
                memcpy(argbytes.bytes + j, &piece, sizeof(uintptr_t));
            }
            uintptr_t aws[2];
            memcpy(aws, &argbytes, sizeof argbytes);
            argdoubleword(aws[0], aws[1]);
        }
        break;

        default:
            debug_printf("unknown argument: %d \n", binding->args[i]);
            break;
        }
        if (a_pos > 7) {
            return LIB_ERR_SLAB_ALLOC_FAIL; // todo
        }
    }
    for (int i = 0; i < binding->n_rets; i++) {
        switch (binding->rets[i]) {
            case AOS_RPC_WORD: {
                argword((ui) &ret[ret_pos++]);
            }
            break;

            case AOS_RPC_CAPABILITY: {
                argword((ui) &retcap);
            }
            break;

            case AOS_RPC_VARSTR: {
                argword((ui) &retstring);
                // TODO err check if used twice
            }
            break;

            case AOS_RPC_VARBYTES: {
                argword((ui) &retbytes);
                // TODO err check if used twice
            }
            break;
            
            default:
            debug_printf("response type not handled\n");
            break;
        }
        if (a_pos > 7) {
            return LIB_ERR_SLAB_ALLOC_FAIL; // todo
        }
    }

    //debug_printf("rpc, handler, n_rets: %p, %p %d\n", rpc, h7, binding->n_rets);
    //debug_printf("calling handler %d with %d retargs: %s\n", binding->msg_type, binding->n_rets, binding->binding_name);
    hd(rpc, arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6],
       stack_args[0], stack_args[1], stack_args[2], stack_args[3],
       stack_args[4], stack_args[5], stack_args[6], stack_args[7],
       stack_args[8], stack_args[9], stack_args[10], stack_args[11],
       stack_args[12], stack_args[13], stack_args[14], stack_args[15]);

    lmi->word_index = 1;
    lmi->msg.words[0] = binding->msg_type | AOS_RPC_RETURN_BIT;
    lmi->cap = NULL_CAP;
    lmi->cap_taken = false;

    ret_pos = 0;

    for (int i = 0; i < binding->n_rets; i++) {
        switch(binding->rets[i]) {
            case AOS_RPC_WORD: {
                uintptr_t word = ret[ret_pos++];
                push_word_lmp(lc, lmi, word);
            }
            break;
            
            case AOS_RPC_CAPABILITY: {
                push_cap_lmp(lc, lmi, retcap);
            }
            break;

            case AOS_RPC_VARSTR: {
                uintptr_t length = strlen(retstring) + 1;
                push_word_lmp(lc, lmi, length);
                for (int j = 0; j < length; j += sizeof(uintptr_t)) {
                    uintptr_t word;
                    memcpy(&word, retstring + j, min(sizeof(uintptr_t), length - j));
                    push_word_lmp(lc, lmi, word);
                }
            }
            break;

            case AOS_RPC_VARBYTES: {
                uintptr_t length = retbytes.length;
                push_word_lmp(lc, lmi, length);
                for (int j = 0; j < length; j += sizeof(uintptr_t)) {
                    uintptr_t word;
                    memcpy(&word, retbytes.bytes + j, min(sizeof(uintptr_t), length - j));
                    push_word_lmp(lc, lmi, word);
                }
            }
            break;

            default:
            debug_printf("unhandled ret arg 3\n");
            break;
        }
    }
    send_remaining_lmp(lc, lmi);

    return SYS_ERR_OK;
}


/* ===================== Retrieve Information ===================== */

struct aos_rpc *aos_rpc_get_init_channel(void)
{
    return get_init_rpc();
}

struct aos_rpc *aos_rpc_get_memory_channel(void)
{
    //TODO: Return channel to talk to memory server process (or whoever
    //implements memory server functionality)
    //debug_printf("aos_rpc_get_memory_channel NYI\n");
    return get_mm_rpc();
}

struct aos_rpc *aos_rpc_get_process_channel(void)
{
    //TODO: Return channel to talk to process server process (or whoever
    //implements process server functionality)
    return get_ns_rpc();
}

/**
 * \brief Returns the channel to the serial console
 */
struct aos_rpc *aos_rpc_get_serial_channel(void)
{
    //TODO: Return channel to talk to serial driver/terminal process (whoever
    //implements print/read functionality)
    debug_printf("aos_rpc_get_serial_channel NYI\n");
    return aos_rpc_get_init_channel();
    return NULL;
}


/* ===================== RPC Requests ===================== */

errval_t aos_rpc_send_number(struct aos_rpc *rpc, uintptr_t num)
{
    return aos_rpc_call(rpc, AOS_RPC_SEND_NUMBER, num);
}

errval_t aos_rpc_send_string(struct aos_rpc *rpc, const char *string)
{
    return aos_rpc_call(rpc, AOS_RPC_SEND_STRING, string);
}

/**
 * \brief Get one character from the serial port
 */
errval_t aos_rpc_serial_getchar(struct aos_rpc *rpc, char *retc)
{
    return aos_rpc_call(rpc, AOS_RPC_GETCHAR, retc);
}

/**
 * \brief Get all characters until EOF/carriage return from the serial port
 * TODO optimize for multiple input
 */
errval_t aos_rpc_get_terminal_input(struct aos_rpc *rpc, char* buf, size_t len)
{
    errval_t err;
    for (int i = 0; i < len; i++) {
        err = aos_rpc_call(rpc, AOS_RPC_GETCHAR, buf + i);
        ON_ERR_RETURN(err);
        if (buf[i] == 13 || buf[i] == '\n') {
            buf[i] = '\0';
            return SYS_ERR_OK;
        }
    }
    return SYS_ERR_OK;
}

/**
 * \brief Send one character to the serial port
 */
errval_t aos_rpc_serial_putchar(struct aos_rpc *rpc, char c)
{
    return aos_rpc_call(rpc, AOS_RPC_PUTCHAR, c);
}
// TODO: aos_rpc_write_terminal_output

/**
 * \brief Request that the process manager start a new process
 *
 * \param cmdline The name of the process that needs to be spawned (without a
 *           path prefix) and optionally any arguments to pass to it
 * \param newpid The process id of the newly-spawned process
 */
errval_t aos_rpc_process_spawn(struct aos_rpc *rpc, char *cmdline, coreid_t core, domainid_t *newpid) {
    // TODO (M5): implement spawn new process rpc
    return aos_rpc_call(rpc, INIT_IFACE_SPAWN, cmdline, core, newpid);
}

/**
 * \brief Get the name of the process with the given PID.
 *
 * \param pid The process id to lookup
 * \param name A null-terminated character array with the name of the process
 * that is allocated by the rpc implementation. Freeing is the caller's
 * responsibility.
 */
errval_t aos_rpc_process_get_name(struct aos_rpc *rpc, domainid_t pid, char **name) {
    errval_t err;
    char * new_name = malloc(8 * 1024 * sizeof(char));
    err = aos_rpc_call(rpc,NS_GET_PROC_NAME,pid,new_name);
    ON_ERR_RETURN(err);
    size_t n = strlen(new_name) + 1;
    *name = realloc(new_name, n * sizeof(char));
    return SYS_ERR_OK;
}

/**
 * \brief Get PIDs of all running processes.
 *
 * \param pids An array containing the process ids of all currently active
 * processes. Will be allocated by the rpc implementation. Freeing is the
 * caller's  responsibility.
 * \param pid_count The number of entries in `pids' if the call was successful
 */
errval_t aos_rpc_process_get_all_pids(struct aos_rpc *rpc, domainid_t **pids, size_t *pid_count) {
    errval_t err;
    char buffer[1024];

    err = aos_rpc_call(rpc,NS_GET_PROC_LIST,pid_count,buffer);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed aos_rpc call in get all pids\n");
    }
    domainid_t* pid_heap = (domainid_t*) malloc(*pid_count * sizeof(domainid_t));
    // debug_printf("Received buffer: %s\n",buffer);
    char * buf_ptr = buffer;
    char strbuf[13];
    size_t index = 0;
    size_t pid_index = 0;
    while(*buf_ptr != '\0'){
        if(*buf_ptr == ','){
            strbuf[index] = '\0';
            domainid_t pid = atoi(strbuf);
            // debug_printf(" word : %s\n",strbuf);
            // debug_printf(" number : %d\n",pid);
            pid_heap[pid_index] = pid;
            pid_index++;
            buf_ptr++;
            index = 0;
        }
        else{
            strbuf[index] = *buf_ptr;
            buf_ptr++;
            index++;
        }
    }
    *pids = pid_heap;
    return SYS_ERR_OK;
}

#include <aos/default_interfaces.h>

/**
 * \brief Request a RAM capability with >= request_bits of size over the given
 * channel.
 */
errval_t aos_rpc_get_ram_cap(struct aos_rpc *rpc, size_t bytes, size_t alignment, struct capref *ret_cap, size_t *ret_bytes) {
    size_t _rs = 0;

    return aos_rpc_call(rpc, MM_IFACE_GET_RAM, bytes, alignment, ret_cap, ret_bytes ? : &_rs);
}

/**
 * \brief Requesting ram via the rpc channel. Must be RPC channel to core 0!
 */
errval_t aos_rpc_request_foreign_ram(struct aos_rpc *rpc, size_t size, struct capref *ret_cap, size_t *ret_size)
{
    errval_t err;
    assert(rpc->backend == AOS_RPC_UMP && "Tried to call foreign ram request on an LMP channel!\n");
    err = aos_rpc_call(rpc,AOS_RPC_REQUEST_RAM, size, 1, ret_cap,ret_size);
    ON_ERR_RETURN(err);
    return err;
}


errval_t aos_rpc_new_binding(domainid_t pid, coreid_t core_id, struct aos_rpc* ret_rpc){
    
    // struct aos_rpc rpc;
    // if(get_init_domain()){
    //     rpc = get_core_channel
    // }


    errval_t err;
    struct capref self_ep_cap = (struct capref) {
      .cnode = cnode_task,
      .slot = TASKCN_SLOT_SELFEP
    };
    debug_printf("Trying to establish connection with process %d on core %d ...\n",pid,core_id);
    // struct aos_rpc *rpc = aos_rpc_get_init_channel();

    if(core_id == disp_get_current_core_id()){ //lmp channel

        struct lmp_endpoint *ep;
        err = endpoint_create(256, &self_ep_cap, &ep);
        ON_ERR_PUSH_RETURN(err, LIB_ERR_ENDPOINT_CREATE);

        //err = aos_rpc_init(ret_rpc);
        //ON_ERR_RETURN(err);

        // TODO (RPC): init interface

        // initialize_general_purpose_handler(ret_rpc);

        err = aos_rpc_init_lmp(ret_rpc, self_ep_cap, NULL_CAP, ep, NULL);
        ON_ERR_RETURN(err);

        struct capref remote_cap;

        err = aos_rpc_call(aos_rpc_get_process_channel(),AOS_RPC_BINDING_REQUEST,pid,core_id,disp_get_current_core_id(),self_ep_cap,&remote_cap);


        // char buf[512];
        // debug_print_cap_at_capref(buf,512,remote_cap);
        // debug_printf("Cap here %s\n",buf);
        ret_rpc -> channel.lmp.remote_cap = remote_cap;
        // *ret_rpc = new_rpc;
        // debug_printf("Channel with %d on %d established!\n",pid,core_id);
    }else{ //ump channel



        debug_printf("Creating new ump channel!\n");
        struct capref urpc_cap;
        size_t urpc_cap_size;

        err  = frame_alloc(&urpc_cap,BASE_PAGE_SIZE,&urpc_cap_size);
        ON_ERR_RETURN(err);

        


        // struct frame_identity urpc_frame_id = (struct frame_identity) {
        //     .base = get_phys_addr(urpc_cap),
        //     .bytes = urpc_cap_size,
        //     .pasid = disp_get_core_id()
        // };

        char *urpc_data = NULL;
        err = paging_map_frame_complete(get_current_paging_state(), (void **) &urpc_data, urpc_cap, NULL, NULL);
        ON_ERR_RETURN(err);



        // TODO (RPC): init interface
        //err = aos_rpc_init(ret_rpc);
        //ON_ERR_RETURN(err);
        
        // struct aos_rpc *rpc = malloc(sizeof(struct aos_rpc));
        // NULLPTR_CHECK(ret_rpc, LIB_ERR_MALLOC_FAIL);

        
        //err = aos_rpc_init(ret_rpc);
        //ON_ERR_RETURN(err);
        // ON_ERR_PUSH_RETURN(err, LIB_ERR_RPC_INIT);

        err =  aos_rpc_init_ump_default(ret_rpc,(lvaddr_t) urpc_data, BASE_PAGE_SIZE,true);//take first half as creating process


        
        ON_ERR_RETURN(err);

        debug_printf("Binding request ...\n");
        struct capref dummy_cap; // not useed
        err = aos_rpc_call(aos_rpc_get_process_channel(),AOS_RPC_BINDING_REQUEST,pid,core_id,disp_get_current_core_id(), urpc_cap,&dummy_cap);
        ON_ERR_RETURN(err);

        debug_printf("Cahnnel estblished on requester!\n");
    }

    //initialize_general_purpose_handler(ret_rpc);
    
    return SYS_ERR_OK;
}


errval_t aos_rpc_new_binding_by_name(char * name, struct aos_rpc * new_rpc){
    errval_t err;
    domainid_t * pids;
    size_t pid_count;
    // debug_printf("here is %lx\n", aos_rpc_get_process_channel());
    err = aos_rpc_process_get_all_pids(aos_rpc_get_process_channel(),&pids,&pid_count);
    ON_ERR_RETURN(err);
    // debug_printf("got all pids!\n");

    // domainid_t pid_buffer
    // for(size_t i = 0; i < pid_count;++i){
    // }
    for(size_t i = 0; i < pid_count; ++i){
        
        domainid_t pid = pids[i];
        // debug_printf("[%d] pid :%d \n",i, pid);
        char* proc_name;
        err = aos_rpc_process_get_name(aos_rpc_get_process_channel(),pid,&proc_name);
        // debug_printf("[%d] pid :%d \n",i, pid);
        // debug_printf("got name of pid!\n");
        ON_ERR_RETURN(err);
        // debug_printf("%s, %s, %d\n",proc_name,name,strcmp(proc_name,name));
        if(strcmp(proc_name,name) == 0){
            coreid_t core_id;
            err = aos_rpc_call(aos_rpc_get_process_channel(),AOS_RPC_GET_PROC_CORE,pid,&core_id);
            // debug_printf("got core id pid!\n");
            ON_ERR_RETURN(err);
            // debug_printf("herer\n");


            // debug_printf("pid: %d, core_id : %d\n",pid,core_id);
            err = aos_rpc_new_binding(pid,core_id,new_rpc);
            ON_ERR_RETURN(err);
            debug_printf("channel established on client!\n");
            return SYS_ERR_OK;
        }
    }
    return PROC_MGMT_ERR_DOMAIN_NOT_RUNNING;
}
