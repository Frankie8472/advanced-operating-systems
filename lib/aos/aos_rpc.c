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

static void aos_rpc_setup_page_handler(struct aos_rpc* rpc, uintptr_t port, uintptr_t size, struct capref frame) {
    debug_printf("aos_rpc_setup_page_handler\n");
    errval_t err = paging_map_frame(get_current_paging_state(), &rpc->bindings[port].buf_page_remote, size, frame, NULL, NULL);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "setting up communication page\n");
    }
}


static errval_t setup_buf_page(struct aos_rpc *rpc, enum aos_rpc_msg_type msg_type, uintptr_t size)
{
    struct capref frame;
    errval_t err;
    
    // create frame to share
    err = frame_alloc(&frame, size, NULL);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_FRAME_ALLOC);

    // map into local address space and store address in rpc->bindings[msg_type].buf_page
    err = paging_map_frame_complete(get_current_paging_state(), &rpc->bindings[msg_type].buf_page, frame, NULL, NULL);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_VSPACE_MAP);

    // call remote end to install shared frame into its own address space
    err = aos_rpc_call(rpc, AOS_RPC_SETUP_PAGE, msg_type, size, frame);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_RPC_SETUP_PAGE); // TODO: New error code

    return SYS_ERR_OK;
}

errval_t aos_rpc_init(struct aos_rpc *rpc)
{
    memset(rpc->bindings, 0, sizeof rpc->bindings);

    // bind initiate function (to send our endpoint to init)
    aos_rpc_initialize_binding(rpc, AOS_RPC_INITIATE, 1, 0, AOS_RPC_CAPABILITY);

    // bind further functions
    aos_rpc_initialize_binding(rpc, AOS_RPC_SEND_NUMBER, 1, 0, AOS_RPC_WORD);
    aos_rpc_initialize_binding(rpc, AOS_RPC_SEND_STRING, 1, 0, AOS_RPC_STR);
    aos_rpc_initialize_binding(rpc, AOS_RPC_REQUEST_RAM, 2, 2, AOS_RPC_WORD, AOS_RPC_WORD, AOS_RPC_CAPABILITY, AOS_RPC_WORD);

    aos_rpc_initialize_binding(rpc, AOS_RPC_SETUP_PAGE, 3, 0, AOS_RPC_WORD, AOS_RPC_WORD, AOS_RPC_CAPABILITY);
    aos_rpc_register_handler(rpc, AOS_RPC_SETUP_PAGE, &aos_rpc_setup_page_handler);

    aos_rpc_initialize_binding(rpc, AOS_RPC_PROC_SPAWN_REQUEST, 2, 1, AOS_RPC_STR, AOS_RPC_WORD, AOS_RPC_WORD);
    aos_rpc_initialize_binding(rpc, AOS_RPC_FOREIGN_SPAWN, 2, 1, AOS_RPC_SHORTSTR, AOS_RPC_WORD, AOS_RPC_WORD);

    aos_rpc_initialize_binding(rpc, AOS_RPC_PUTCHAR, 1, 0, AOS_RPC_WORD);
    aos_rpc_initialize_binding(rpc, AOS_RPC_GETCHAR, 0, 1, AOS_RPC_WORD);

    aos_rpc_initialize_binding(rpc, AOS_RPC_ROUNDTRIP, 0, 0);
    
    return SYS_ERR_OK;
}

/**
 * NOTE: maybe add remote and local cap as parameters?
 *       atm they have to be set before calling dis method
 * i.e. rpc->channel.local_cap / remote_cap are used here
 * and should be properly set beforehand
 * \brief Initialize an aos_rpc struct.
 */
errval_t aos_rpc_init_lmp(struct aos_rpc* rpc, struct capref self_ep, struct capref end_ep, struct lmp_endpoint *lmp_ep) {
    errval_t err;
    debug_printf("aos_rpc_init\n");
    
    rpc->backend = AOS_RPC_LMP;

    lmp_chan_init(&rpc->channel.lmp);
    rpc->channel.lmp.local_cap = self_ep;
    rpc->channel.lmp.remote_cap = end_ep;

    if (lmp_ep == NULL) {
        err = endpoint_create(256, &rpc->channel.lmp.local_cap, &lmp_ep);
        ON_ERR_PUSH_RETURN(err, LIB_ERR_ENDPOINT_CREATE);
    }

    rpc->channel.lmp.buflen_words = 256;
    rpc->channel.lmp.endpoint = lmp_ep;
    
    err = lmp_chan_alloc_recv_slot(&rpc->channel.lmp);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_LMP_ALLOC_RECV_SLOT);


    err = lmp_chan_register_recv(&rpc->channel.lmp, get_default_waitset(), MKCLOSURE(&aos_rpc_on_message, rpc));
    ON_ERR_PUSH_RETURN(err, LIB_ERR_LMP_ENDPOINT_REGISTER);

    if (!capref_is_null(rpc->channel.lmp.remote_cap)) {
        // we are not in init and do already have a remote cap
        // we need to initiate a connection so init gets our endpoint capability
        debug_printf("Trying to establish channel with init:\n");

        // call initiate function with our endpoint cap as argument in order
        // to make it known to init
        err = aos_rpc_call(rpc, AOS_RPC_INITIATE, rpc->channel.lmp.local_cap);
        if (!err_is_fail(err)) {
            debug_printf("init channel established!\n");
        }
        else {
            DEBUG_ERR(err, "error establishing connection with init.\n");
            ON_ERR_PUSH_RETURN(err, LIB_ERR_RPC_INITIATE);
        }
    }

    return err;
}


errval_t aos_rpc_init_ump(struct aos_rpc *rpc, lvaddr_t shared_page, size_t shared_page_size, bool first_half)
{
    errval_t err;

    rpc->backend = AOS_RPC_UMP;
    
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
    err = ump_chan_register_polling(ump_chan_get_default_poller(), &rpc->channel.ump, &aos_rpc_on_ump_message, rpc);
    ON_ERR_RETURN(err);
    
    return SYS_ERR_OK;
}


errval_t
aos_rpc_send_number(struct aos_rpc *rpc, uintptr_t num) {
    return aos_rpc_call(rpc, AOS_RPC_SEND_NUMBER, num);
}

errval_t
aos_rpc_send_string(struct aos_rpc *rpc, const char *string)
{
    return aos_rpc_call(rpc, AOS_RPC_SEND_STRING, string);
}

errval_t
aos_rpc_get_ram_cap(struct aos_rpc *rpc, size_t bytes, size_t alignment,
                    struct capref *ret_cap, size_t *ret_bytes) {
    size_t _rs = 0;

    return aos_rpc_call(rpc, AOS_RPC_REQUEST_RAM, bytes, alignment, ret_cap, ret_bytes ? : &_rs);
    debug_printf("here: ");
    HERE;
}

errval_t
aos_rpc_serial_getchar(struct aos_rpc *rpc, char *retc)
{
    return aos_rpc_call(rpc, AOS_RPC_GETCHAR, retc);
}

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

errval_t
aos_rpc_serial_putchar(struct aos_rpc *rpc, char c)
{
    return aos_rpc_call(rpc, AOS_RPC_PUTCHAR, c);
}

errval_t
aos_rpc_process_spawn(struct aos_rpc *rpc, char *cmdline,
                      coreid_t core, domainid_t *newpid) {
    // TODO (M5): implement spawn new process rpc
    return aos_rpc_call(rpc, AOS_RPC_PROC_SPAWN_REQUEST, cmdline, core, newpid);
}



errval_t
aos_rpc_process_get_name(struct aos_rpc *rpc, domainid_t pid, char **name) {
    // TODO (M5): implement name lookup for process given a process id
    return SYS_ERR_OK;
}


errval_t
aos_rpc_process_get_all_pids(struct aos_rpc *rpc, domainid_t **pids,
                             size_t *pid_count) {
    // TODO (M5): implement process id discovery
    return SYS_ERR_OK;
}


static errval_t aos_rpc_unmarshall_retval_aarch64(
                        void **retptrs, struct aos_rpc_function_binding *binding,
                        struct lmp_recv_msg *msg, struct capref cap)
{
    int msg_ind = 1;
    for (int i = 0; i < binding->n_rets; i++) {
        if (binding->rets[i] == AOS_RPC_WORD) {
            *((uintptr_t*)retptrs[i]) = msg->words[msg_ind];
        }
        else if (binding->rets[i] == AOS_RPC_CAPABILITY) {
            *((struct capref*) retptrs[i]) = cap;
        }
        else if (binding->rets[i] == AOS_RPC_STR) {
            // TODO: @frnz
        }
    }
    return SYS_ERR_OK;
}


static errval_t aos_rpc_call_ump(struct aos_rpc *rpc, enum aos_rpc_msg_type msg_type, va_list args)
{
    assert(rpc);
    assert(rpc->backend == AOS_RPC_UMP);

    struct aos_rpc_function_binding *binding = &rpc->bindings[msg_type];
    size_t n_args = binding->n_args;
    size_t n_rets = binding->n_rets;
    void* retptrs[4];

    struct ump_msg um = {
        .flag = 0
    };

    um.data.u64[0] = msg_type;

    int word_ind = 1;
    int ret_ind = 0;
    for (int i = 0; i < n_args; i++) {
        if (binding->args[i] == AOS_RPC_WORD) {
            um.data.u64[word_ind++] = va_arg(args, uintptr_t);
        }
        else if (binding->args[i] == AOS_RPC_SHORTSTR) {
            const char *str = va_arg(args, char*);
            assert(strlen(str) < AOS_RPC_SHORTSTR_LENGTH);
            strncpy((char *) &um.data.u64[word_ind], str, AOS_RPC_SHORTSTR_LENGTH);
            word_ind += AOS_RPC_SHORTSTR_LENGTH / sizeof(uintptr_t);
        }
        else if (binding->args[i] == AOS_RPC_CAPABILITY) {
            struct capref cr = va_arg(args, struct capref);
            struct capability cap;
            // non-portable assertion
            static_assert(sizeof(struct capability) == 3 * sizeof(uintptr_t));
            invoke_cap_identify(cr, &cap);
            memcpy(&um.data.u64[word_ind], &cap, sizeof(struct capability));
            word_ind += sizeof(struct capability) / sizeof(uintptr_t);
        }
        else {
            debug_printf("non-word or shortstring messages over ump NYI!\n");
            return LIB_ERR_NOT_IMPLEMENTED;
        }
    }
    for (int i = 0; i < n_rets; i++) {
        retptrs[ret_ind++] = va_arg(args, void*);
    }

    bool sent = false;
    do {
        sent = ump_chan_send(&rpc->channel.ump, &um);
    } while (!sent);


    struct ump_msg response;
    bool received = false;
    do {
        received = ump_chan_poll_once(&rpc->channel.ump, &response);
    } while(!received);

    if ((response.data.u64[0] | AOS_RPC_RETURN_BIT) && (response.data.u64[0] & ~AOS_RPC_RETURN_BIT) == msg_type) {
        // okay
    }
    else {
        return LIB_ERR_NOT_IMPLEMENTED; // todo errcode
    }
    
    debug_printf("response words: %ld %ld %ld %ld %ld %ld %ld\n",
        response.data.u64[0],
        response.data.u64[1],
        response.data.u64[2],
        response.data.u64[3],
        response.data.u64[4],
        response.data.u64[5],
        response.data.u64[6]
    );

    size_t ret_offs = 1;
    for (int i = 0; i < n_rets; i++) {
        if (binding->rets[i] == AOS_RPC_WORD) {
            *((uintptr_t *) retptrs[i]) = response.data.u64[ret_offs++];
        }
        else if (binding->rets[i] == AOS_RPC_SHORTSTR) {
            strncpy(((char *) retptrs[i]), (const char *) &response.data.u64[ret_offs], AOS_RPC_SHORTSTR_LENGTH);
            ret_offs += AOS_RPC_SHORTSTR_LENGTH / sizeof(uintptr_t);
        }
        else if (binding->rets[i] == AOS_RPC_CAPABILITY) {
            struct capability cap;
            memcpy(&cap, &response.data.u64[ret_offs], sizeof cap);
            ret_offs += sizeof(struct capability) / sizeof(uintptr_t);

            struct capref forged;
            errval_t err = slot_alloc(&forged);
            ON_ERR_PUSH_RETURN(err, LIB_ERR_SLOT_ALLOC);

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
        else {
            debug_printf("non-word responses over ump NYI!\n");
            return LIB_ERR_NOT_IMPLEMENTED;
        }
    }

    return SYS_ERR_OK;
}


errval_t aos_rpc_call(struct aos_rpc *rpc, enum aos_rpc_msg_type msg_type, ...)
{
    va_list args;

    errval_t err;

    if (rpc->backend == AOS_RPC_UMP) {
        va_start(args, msg_type);
        return aos_rpc_call_ump(rpc, msg_type, args);
    }

    struct aos_rpc_function_binding *binding = &rpc->bindings[msg_type];
    size_t n_args = binding->n_args;
    size_t n_rets = binding->n_rets;
    void* retptrs[4];
    if (binding->calling_simple) {
        assert(n_args < LMP_MSG_LENGTH);
        uintptr_t words[LMP_MSG_LENGTH];
        struct capref cap = NULL_CAP;
        int word_ind = 0;
        int ret_ind = 0;
        size_t buf_page_offset = 0;
        va_start(args, msg_type);
        for (int i = 0; i < n_args; i++) {
            if (binding->args[i] == AOS_RPC_WORD) {
                words[word_ind++] = va_arg(args, uintptr_t);
            }
            else if (binding->args[i] == AOS_RPC_CAPABILITY) {
                cap = va_arg(args, struct capref);
            }
            else if (binding->args[i] == AOS_RPC_STR) {
                const char* str = va_arg(args, char*);
                if (binding->buf_page == NULL) {
                    // TODO: implement size check for string
                    err = setup_buf_page(rpc, msg_type, BASE_PAGE_SIZE * 4);
                    ON_ERR_PUSH_RETURN(err, LIB_ERR_FRAME_ALLOC); // Todo: create new error code
                }
                // send the offset into the buf_page
                words[word_ind++] = buf_page_offset;
                size_t length = strlen(str);
                if (length + buf_page_offset >= BASE_PAGE_SIZE * 4) length = BASE_PAGE_SIZE * 4 - buf_page_offset;
                strncpy(binding->buf_page + buf_page_offset, str, length);
                buf_page_offset += length;
            }
        }
        for (int i = 0; i < n_rets; i++) {
            retptrs[ret_ind++] = va_arg(args, void*);
        }
        va_end(args);
        //debug_printf("sending call %ld, %ld, %ld, %ld\n", msg_type, words[0], words[1], words[2]);
        lmp_chan_send(&rpc->channel.lmp, LMP_SEND_FLAGS_DEFAULT, cap, n_args + 1,
                    msg_type, words[0], words[1], words[2]);
    }
    else {
        debug_printf("unknown binding\n");
    }
    //debug_printf("waiting for response\n");

    while(!lmp_chan_can_recv(&rpc->channel.lmp)) {
        //dispatcher_handle_t d = disp_disable();
        //sys_yield(CPTR_NULL);
        //disp_enable(d);
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

    err = aos_rpc_unmarshall_retval_aarch64(retptrs, binding, &msg, recieved_cap);
    ON_ERR_RETURN(err);

    return SYS_ERR_OK;
}


errval_t aos_rpc_register_handler(struct aos_rpc *rpc, enum aos_rpc_msg_type binding, void* handler)
{
    rpc->handlers[binding] = handler;
    return SYS_ERR_OK;
}


errval_t aos_rpc_initialize_binding(struct aos_rpc *rpc, enum aos_rpc_msg_type binding,
                                    int n_args, int n_rets, ...)
{
    struct aos_rpc_function_binding *fb = &rpc->bindings[binding];
    va_list args;
    fb->n_args = n_args;
    fb->n_rets = n_rets;
    fb->port = binding;

    int wordargs = 0;
    int capargs = 0;

    int wordrets = 0;
    int caprets = 0;

    va_start(args, n_rets);
    for (int i = 0; i < n_args; i++) {
        fb->args[i] = va_arg(args, int); // read enum values promoted to int
        if (fb->args[i] == AOS_RPC_WORD) wordargs++;
        if (fb->args[i] == AOS_RPC_STR) wordargs++;
        if (fb->args[i] == AOS_RPC_CAPABILITY) capargs++;
    }
    for (int i = 0; i < n_rets; i++) {
        fb->rets[i] = va_arg(args, int); // read enum values promoted to int
        if (fb->args[i] == AOS_RPC_WORD) wordrets++;
        if (fb->args[i] == AOS_RPC_STR) wordrets++;
        if (fb->args[i] == AOS_RPC_CAPABILITY) caprets++;
    }
    va_end(args);

    fb->calling_simple = wordargs <= 3 && capargs <= 1;
    fb->returning_simple = wordrets <= 3 && caprets <= 1;

    if (!fb->calling_simple || !fb->returning_simple) {
        debug_printf("advanced rpc marshalling not yet implemented\n");
        return LIB_ERR_NOT_IMPLEMENTED;
    }

    return SYS_ERR_OK;
}


static errval_t aos_rpc_unmarshall_simple_aarch64(struct aos_rpc *rpc,
                        void *handler, struct aos_rpc_function_binding *binding,
                        struct lmp_recv_msg *msg, struct capref cap)
{
    //debug_printf("words: %ld %ld %ld %ld\n", msg->data.u64[0], msg->data.u64[1], msg->data.u64[2], msg->data.u64[3]);

    typedef uintptr_t ui;
    ui arg[8] = { 0 }; // Upper bound so only writes in register
    ui ret[4] = { 0 };
    struct capref retcap = NULL_CAP;
    errval_t (*h4)(struct aos_rpc*, ui, ui, ui, ui) = handler;
    errval_t (*h7)(struct aos_rpc*, ui, ui, ui, ui, ui, ui, ui, ui) = handler;
    int a_pos = 0;
    int buf_pos = 1;
    int ret_pos = 0;
    for (int i = 0; i < binding->n_args; i++) {
        if (binding->args[i] == AOS_RPC_WORD) {
            arg[a_pos++] = msg->words[buf_pos++];
        }
        else if (binding->args[i] == AOS_RPC_CAPABILITY) {
            arg[a_pos++] = (cap.cnode.croot) | (((ui)cap.cnode.cnode) << 32);
            arg[a_pos++] = (cap.cnode.level) | (((ui)cap.slot) << 32);
        }
        else if (binding->args[i] == AOS_RPC_STR) {
            arg[a_pos++] = ((ui) binding->buf_page_remote) + msg->words[buf_pos++];
        }
        else {
            // TODO: error, can't transmit strings or other over simple channels
        }
        if (a_pos >= 9) {
            return LIB_ERR_SLAB_ALLOC_FAIL; // todo
        }
    }
    for (int i = 0; i < binding->n_rets; i++) {
        if (binding->rets[i] == AOS_RPC_WORD) {
            arg[a_pos++] = (ui) &ret[ret_pos++];
        }
        else if (binding->rets[i] == AOS_RPC_CAPABILITY) {
            arg[a_pos++] = (ui) &retcap;
        }
        else if (binding->args[i] == AOS_RPC_STR) { // TODO: TEST!
            arg[a_pos++] = (ui) &ret[ret_pos++];
        }
        else {
            // TODO: error, can't transmit strings or other over simple channels
        }
        if (a_pos >= 9) {
            return LIB_ERR_SLAB_ALLOC_FAIL; // todo
        }
    }

    if (a_pos == 4) {
        h4(rpc, arg[0], arg[1], arg[2], arg[3]);
    }
    else {
        h7(rpc, arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6], arg[7]);
    }

    // send response
    errval_t err = lmp_chan_send(&rpc->channel.lmp, LMP_SEND_FLAGS_DEFAULT, retcap, ret_pos + 1, binding->port | AOS_RPC_RETURN_BIT, ret[0], ret[1], ret[2]);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_LMP_CHAN_SEND);

    return SYS_ERR_OK;
}


void aos_rpc_on_message(void *arg)
{
    //debug_printf("aos_rpc_on_message\n");
    struct aos_rpc *rpc = arg;
    struct lmp_chan *channel = &rpc->channel.lmp;

    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref recieved_cap = NULL_CAP;
    errval_t err;

    err = lmp_chan_recv(channel, &msg, &recieved_cap);
    if (err_is_fail(err) && lmp_err_is_transient(err)) {
        debug_printf("transient error\n");
        err = lmp_chan_register_recv(channel, get_default_waitset(), MKCLOSURE(&aos_rpc_on_message, arg));
    }
    else if (err_is_fail(err)) {
        err = err_push(err, LIB_ERR_LMP_CHAN_RECV);
        goto on_error;
    }

    if (!capref_is_null(recieved_cap)) {
        lmp_chan_alloc_recv_slot(channel);
    }

    uintptr_t msgtype = msg.words[0];

    bool is_response = false;
    if (msgtype & AOS_RPC_RETURN_BIT) {
        msgtype &= ~AOS_RPC_RETURN_BIT;
        is_response = true;
    }

    void *handler = rpc->handlers[msgtype];
    if (handler == NULL) {
        debug_printf("no handler for %d\n", msgtype);
        err = LIB_ERR_RPC_NO_HANDLER_SET;
        goto on_error;
    }

    struct aos_rpc_function_binding *binding = &rpc->bindings[msgtype];

    // in case of simple binding
    if (binding->calling_simple) {
        err = aos_rpc_unmarshall_simple_aarch64(rpc, handler, binding, &msg, recieved_cap);
        if (err_is_fail(err)) {
            goto on_error;
        }
    }
    else {
        debug_printf("NYI in aos_rpc_on_message\n");
    }


//on_success:
    //debug_printf("reregister\n");
    err = lmp_chan_register_recv(channel, get_default_waitset(), MKCLOSURE(&aos_rpc_on_message, arg));
    return;

    errval_t err2;
on_error:
    err2 = lmp_chan_register_recv(channel, get_default_waitset(), MKCLOSURE(&aos_rpc_on_message, arg));
    DEBUG_ERR(err, "error handling message\n");
    return;
}


static errval_t aos_rpc_unmarshall_ump_simple_aarch64(struct aos_rpc *rpc,
                        void *handler, struct aos_rpc_function_binding *binding,
                        struct ump_msg *msg)
{
    debug_printf("words: %ld %ld %ld %ld %ld %ld %ld\n",
        msg->data.u64[0], msg->data.u64[1], msg->data.u64[2], msg->data.u64[3],
        msg->data.u64[4], msg->data.u64[5], msg->data.u64[6]
    );

    typedef uintptr_t ui;
    ui arg[8] = { 0 }; // Upper bound so only writes in register
    ui ret[8] = { 0 };
    errval_t (*h8)(struct aos_rpc*, ui, ui, ui, ui, ui, ui, ui, ui) = handler;
    int a_pos = 0;
    int buf_pos = 1;
    int ret_pos = 0;
    struct capref retcap;
    for (int i = 0; i < binding->n_args; i++) {
        if (a_pos >= 8) {
            return LIB_ERR_SLAB_ALLOC_FAIL; // todo create better error code
        }
        if (binding->args[i] == AOS_RPC_WORD) {
            arg[a_pos++] = msg->data.u64[buf_pos++];
        }
        else if (binding->args[i] == AOS_RPC_SHORTSTR) {
            arg[a_pos++] = (ui) &msg->data.u64[buf_pos];
            buf_pos += 4;
        }
        else if (binding->args[i] == AOS_RPC_CAPABILITY) {
            struct capability cap;
            memcpy(&cap, &msg->data.u64[buf_pos], sizeof cap);
            buf_pos += sizeof(struct capability) / sizeof(uintptr_t);

            struct capref forged;
            errval_t err = slot_alloc(&forged);
            ON_ERR_PUSH_RETURN(err, LIB_ERR_SLOT_ALLOC);

            err = invoke_monitor_create_cap((uint64_t *) &cap,
                                            get_cnode_addr(forged),
                                            get_cnode_level(forged),
                                            forged.slot, !disp_get_core_id()); // TODO: set owner correctly
            ON_ERR_PUSH_RETURN(err, LIB_ERR_MONITOR_CAP_SEND);

            arg[a_pos++] = (forged.cnode.croot) | (((ui) forged.cnode.cnode) << 32);
            arg[a_pos++] = (forged.cnode.level) | (((ui) forged.slot) << 32);
        }
        else {
            debug_printf("only integer and shortstr arguments supported at the moment\n");
            return LIB_ERR_NOT_IMPLEMENTED;
        }
    }
    bool retcap_used = false;
    for (int i = 0; i < binding->n_rets; i++) {
        if (a_pos >= 6) {
            return LIB_ERR_SLAB_ALLOC_FAIL; // todo
        }
        if (binding->rets[i] == AOS_RPC_WORD) {
            arg[a_pos++] = (ui) &ret[ret_pos++];
        }
        else if (binding->rets[i] == AOS_RPC_SHORTSTR) {
            arg[a_pos++] = (ui) &ret[ret_pos];
            ret_pos += 4;
        }
        else if (binding->rets[i] == AOS_RPC_CAPABILITY) {
            if (retcap_used) return LIB_ERR_NOT_IMPLEMENTED; // error, sending multiple caps nyi
            arg[a_pos++] = (ui) &retcap;
        }
        else {
            debug_printf("only integer and shortstr arguments supported at the moment %d\n", binding->args[i]);
            return LIB_ERR_NOT_IMPLEMENTED;
        }
        if (ret_pos >= 7) {
            return LIB_ERR_SLAB_ALLOC_FAIL; // todo
        }
    }

    h8(rpc, arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6], arg[7]);

    // send response
    struct ump_msg response;
    response.data.u64[0] = msg->data.u64[0] | AOS_RPC_RETURN_BIT;

    buf_pos = 1;
    ret_pos = 0;
    for (int i = 0; i < binding->n_rets; i++) {

        if (binding->rets[i] == AOS_RPC_WORD) {
            response.data.u64[buf_pos++] = ret[ret_pos++];
        }
        else if (binding->rets[i] == AOS_RPC_SHORTSTR) {
            strncpy((char *) &response.data.u64[buf_pos], (const char *) &ret[ret_pos], AOS_RPC_SHORTSTR_LENGTH);
            ret_pos += AOS_RPC_SHORTSTR_LENGTH / sizeof(uintptr_t);
        }
        else if (binding->rets[i] == AOS_RPC_CAPABILITY) {
            struct capability cap;
            errval_t err = invoke_cap_identify(retcap, &cap);
            ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_IDENTIFY);
            memcpy(&response.data.u64[buf_pos], &cap, sizeof cap);
            buf_pos += sizeof(struct capability) / sizeof(uintptr_t);
        }
    }

    bool sent = false;
    do {
        sent = ump_chan_send(&rpc->channel.ump, &response);
    } while(!sent);

    //ON_ERR_PUSH_RETURN(err, LIB_ERR_UMP_CHAN_SEND);

    return SYS_ERR_OK;
}

errval_t aos_rpc_on_ump_message(void *arg, struct ump_msg *msg)
{
    errval_t err;

    struct aos_rpc *rpc = arg;
    enum aos_rpc_msg_type msgtype = msg->data.u64[0];

    void *handler = rpc->handlers[msgtype];
    if (handler == NULL) {
        debug_printf("no handler for %d\n", msgtype);
        err = LIB_ERR_RPC_NO_HANDLER_SET;
        return err;
    }

    struct aos_rpc_function_binding *binding = &rpc->bindings[msgtype];

    // in case of simple binding
    if (binding->calling_simple) {
        err = aos_rpc_unmarshall_ump_simple_aarch64(rpc, handler, binding, msg);
        if (err_is_fail(err)) {
            DEBUG_ERR(err, "error in unmarshall\n");
        }
    }
    else {
        debug_printf("NYI in aos_rpc_on_ump_message\n");
    }
    
    return SYS_ERR_OK;
}




/**
 * \brief Returns the RPC channel to init.
 */
struct aos_rpc *aos_rpc_get_init_channel(void)
{
    return get_init_rpc();
}

/**
 * \brief Returns the channel to the memory server
 */
struct aos_rpc *aos_rpc_get_memory_channel(void)
{
    //TODO: Return channel to talk to memory server process (or whoever
    //implements memory server functionality)
    //debug_printf("aos_rpc_get_memory_channel NYI\n");
    return get_init_rpc();
}

/**
 * \brief Returns the channel to the process manager
 */
struct aos_rpc *aos_rpc_get_process_channel(void)
{
    //TODO: Return channel to talk to process server process (or whoever
    //implements process server functionality)
    debug_printf("aos_rpc_get_process_channel NYI\n");
    return aos_rpc_get_init_channel();
    return NULL;
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


errval_t aos_rpc_request_foreign_ram(struct aos_rpc * rpc, size_t size,struct capref *ret_cap,size_t * ret_size){
    debug_printf("Got here!\n");
    errval_t err;
    assert(rpc -> backend == AOS_RPC_UMP && "Tried to call foreign ram request on an LMP channel!\n");
    err = aos_rpc_call(rpc,AOS_RPC_REQUEST_RAM,size,1,ret_cap,ret_size);
    ON_ERR_RETURN(err);
    return err;

}