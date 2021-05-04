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
    aos_rpc_initialize_binding(rpc, AOS_RPC_SEND_STRING, 1, 0, AOS_RPC_VARSTR);
    aos_rpc_initialize_binding(rpc, AOS_RPC_REQUEST_RAM, 2, 2, AOS_RPC_WORD, AOS_RPC_WORD, AOS_RPC_CAPABILITY, AOS_RPC_WORD);

    aos_rpc_initialize_binding(rpc, AOS_RPC_SETUP_PAGE, 3, 0, AOS_RPC_WORD, AOS_RPC_WORD, AOS_RPC_CAPABILITY);
    aos_rpc_register_handler(rpc, AOS_RPC_SETUP_PAGE, &aos_rpc_setup_page_handler);

    aos_rpc_initialize_binding(rpc, AOS_RPC_PROC_SPAWN_REQUEST, 2, 1, AOS_RPC_VARSTR, AOS_RPC_WORD, AOS_RPC_WORD);
    aos_rpc_initialize_binding(rpc, AOS_RPC_FOREIGN_SPAWN, 2, 1, AOS_RPC_VARSTR, AOS_RPC_WORD, AOS_RPC_WORD);

    aos_rpc_initialize_binding(rpc, AOS_RPC_PUTCHAR, 1, 0, AOS_RPC_WORD);
    aos_rpc_initialize_binding(rpc, AOS_RPC_GETCHAR, 0, 1, AOS_RPC_WORD);

    aos_rpc_initialize_binding(rpc, AOS_RPC_ROUNDTRIP, 0, 0);

    //Process manager bindings
    aos_rpc_initialize_binding(rpc,AOS_RPC_PM_ONLINE,0,0);
    aos_rpc_initialize_binding(rpc,AOS_RPC_REGISTER_PROCESS,3,0,AOS_RPC_WORD,AOS_RPC_WORD, AOS_RPC_VARSTR);
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
            debug_printf("error establishing connection with init.\n");
            ON_ERR_PUSH_RETURN(err, LIB_ERR_RPC_INITIATE);
        }
    }

    return err;
}


/**
 * \brief TODO
 * - inits ump channel with default message size
 */
errval_t aos_rpc_init_ump_default(struct aos_rpc *rpc, lvaddr_t shared_page, size_t shared_page_size, bool first_half)
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
    ump_chan_register_recv(&rpc->channel.ump, get_default_waitset(), MKCLOSURE(&aos_rpc_on_ump_message, rpc));
    //err = ump_chan_register_polling(ump_chan_get_default_poller(), &rpc->channel.ump, &aos_rpc_on_ump_message, rpc);
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


static uintptr_t pull_word_lmp(struct lmp_chan *lc, struct lmp_recv_msg *lm, struct capref *cap, int *word_ind)
{
    if (*word_ind >= LMP_MSG_LENGTH) {
        bool can_receive = lmp_chan_can_recv(lc);
        bool received = false;
        
        while(!can_receive) {
            thread_yield_dispatcher(lc->remote_cap);
            can_receive = lmp_chan_can_recv(lc);
        }

        do {
            errval_t err = lmp_chan_recv(lc, lm, cap);
            received = err == SYS_ERR_OK;
        } while(!received);
        
        *word_ind = 0;
        if (!capref_is_null(*cap)) {
            debug_printf("realloccing slot\n");
            lmp_chan_alloc_recv_slot(lc);
        }
    }
    uintptr_t word = lm->words[*word_ind];
    (*word_ind)++;

    return word;
}


static struct capref pull_cap_lmp(struct lmp_chan *lc, struct lmp_recv_msg *lm, struct capref *cap, int *word_ind)
{
    if (capref_is_null(*cap)) {
        bool can_receive = lmp_chan_can_recv(lc);
        bool received = false;
        while(!can_receive) {
            thread_yield_dispatcher(lc->remote_cap);
            can_receive = lmp_chan_can_recv(lc);
        }

        do {
            errval_t err = lmp_chan_recv(lc, lm, cap);
            received = err == SYS_ERR_OK;
        } while(!received);

        *word_ind = 0;
        if (!capref_is_null(*cap)) {
            debug_printf("realloccing slot\n");
            lmp_chan_alloc_recv_slot(lc);
        }
    }
    struct capref ret = *cap;
    *cap = NULL_CAP;
    return ret;
}


static errval_t aos_rpc_unmarshall_retval_aarch64(struct aos_rpc *rpc,
                        void **retptrs, struct aos_rpc_function_binding *binding,
                        struct lmp_recv_msg *msg, struct capref cap)
{
    struct lmp_chan *lc = &rpc->channel.lmp;
    int msg_ind = 1;
    for (int i = 0; i < binding->n_rets; i++) {
        switch (binding->rets[i]) {
            case AOS_RPC_WORD: {
                *((uintptr_t *) retptrs[i]) = pull_word_lmp(lc, msg, &cap, &msg_ind);
            }
            break;

            case AOS_RPC_CAPABILITY: {
                *((struct capref *) retptrs[i]) = pull_cap_lmp(lc, msg, &cap, &msg_ind);
            }
            break;

            case AOS_RPC_VARSTR: {
                size_t length = pull_word_lmp(lc, msg, &cap, &msg_ind);
                char *str = (char *) retptrs[i];
                for (size_t j = 0; j < length; j += sizeof(uintptr_t)) {
                    uintptr_t word = pull_word_lmp(lc, msg, &cap, &msg_ind);
                    memcpy(str + j, &word, sizeof word);
                }
            }
            break;
            
            default:
            debug_printf("unknown return type in unmarshalling\n");
            break;
        }
    }
    return SYS_ERR_OK;
}

/**
 * \brief
 *
 * \param uc
 * \param um
 * \param word_ind
 * \return
 */
static void push_words(struct ump_chan *uc, struct ump_msg *um, int *word_ind, uintptr_t word)
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


static void send_remaining(struct ump_chan *uc, struct ump_msg *um, int *word_ind)
{
    if (*word_ind >= 0) {
        bool sent = false;
        do {
            sent = ump_chan_send(uc, um);
        } while (!sent);
        *word_ind = 0;
    }
}


static errval_t aos_rpc_call_ump(struct aos_rpc *rpc, enum aos_rpc_msg_type msg_type, va_list args)
{
    assert(rpc);
    assert(rpc->backend == AOS_RPC_UMP);

    struct aos_rpc_function_binding *binding = &rpc->bindings[msg_type];
    size_t n_args = binding->n_args;
    size_t n_rets = binding->n_rets;
    void* retptrs[4];

    /* struct ump_msg um = DECLARE_MESSAGE(rpc->channel.ump); */
    DECLARE_MESSAGE(rpc->channel.ump, um);
    um->flag = 0;

    um->data[0] = msg_type;

    int word_ind = 1;
    int ret_ind = 0;
    for (int i = 0; i < n_args; i++) {
        if (binding->args[i] == AOS_RPC_WORD) {
            push_words(&rpc->channel.ump, um, &word_ind, va_arg(args, uintptr_t));
        }
        else if (binding->args[i] == AOS_RPC_SHORTSTR) {
            const int n_words = AOS_RPC_SHORTSTR_LENGTH / sizeof(uintptr_t);
            uintptr_t words[n_words];

            const char *str = va_arg(args, char*);
            assert(strlen(str) < AOS_RPC_SHORTSTR_LENGTH);

            memcpy(&words, str, strlen(str));
            for (int j = 0; j < n_words; j++) {
                push_words(&rpc->channel.ump, um, &word_ind, words[j]);
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
                push_words(&rpc->channel.ump, um, &word_ind, words[j]);
            }
        }
        else if (binding->args[i] == AOS_RPC_VARSTR) {
            // check for str, set flag in accordance with last element of sended shortstr
            const char *str = va_arg(args, char*);
            size_t msg_len = strlen(str) + 1;
            push_words(&rpc->channel.ump, um, &word_ind, msg_len);
            for (int j = 0; j < msg_len; j += sizeof(uintptr_t)) {
                uintptr_t word = 0;
                memcpy(&word, str + j, sizeof(uintptr_t));
                push_words(&rpc->channel.ump, um, &word_ind, word); // TODO: add check so we don't read over string end
            }
        }
        else {
            debug_printf("non-word or shortstring messages over ump NYI!\n");
            return LIB_ERR_NOT_IMPLEMENTED;
        }
    }

    for (int i = 0; i < n_rets; i++) {
        retptrs[ret_ind++] = va_arg(args, void*);
    }

    send_remaining(&rpc->channel.ump, um, &word_ind);
    

    // Receive
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

    size_t ret_offs = 1;
    for (int i = 0; i < n_rets; i++) {
        if (binding->rets[i] == AOS_RPC_WORD) {
            *((uintptr_t *) retptrs[i]) = response->data[ret_offs++];
        }
        else if (binding->rets[i] == AOS_RPC_SHORTSTR) {
            strncpy(((char *) retptrs[i]), (const char *) &response->data[ret_offs], AOS_RPC_SHORTSTR_LENGTH);
            ret_offs += AOS_RPC_SHORTSTR_LENGTH / sizeof(uintptr_t);
        }
        else if (binding->rets[i] == AOS_RPC_CAPABILITY) {
            struct capability cap;
            memcpy(&cap, &response->data[ret_offs], sizeof cap);
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


static void push_word_lmp(struct lmp_chan *lc, uintptr_t *lm, struct capref *cap, int *word_ind, uintptr_t word)
{
    lm[(*word_ind)++] = word;

    if (*word_ind >= LMP_MSG_LENGTH) {
        bool sent = false;
        do {
            errval_t err = lmp_chan_send(lc, LMP_SEND_FLAGS_DEFAULT, *cap, 4,
                        lm[0], lm[1], lm[2], lm[3]);
            sent = err == SYS_ERR_OK;
        } while (!sent);
        *word_ind = 0;
    }
}


static void push_cap_lmp(struct lmp_chan *lc, uintptr_t *lm, struct capref *cap, int *word_ind, struct capref to_push)
{
    if (!capref_is_null(*cap)) {
        debug_printf("should not happen\n");
        bool sent = false;
        do {
            errval_t err = lmp_chan_send(lc, LMP_SEND_FLAGS_DEFAULT, *cap, 4,
                        lm[0], lm[1], lm[2], lm[3]);
            sent = err == SYS_ERR_OK;
        } while (!sent);
        *word_ind = 0;
    }

    *cap = to_push;
}


static void send_remaining_lmp(struct lmp_chan *lc, uintptr_t *lm, struct capref *cap, int *word_ind)
{
    if (*word_ind >= 0 || !capref_is_null(*cap)) {
        bool sent = false;
        do {
            char buf[128];
            debug_print_cap_at_capref(buf, 128, *cap);
            //debug_printf("sending: %ld %ld %ld %ld, %s\n", lm[0], lm[1], lm[2], lm[3], buf);
            errval_t err = lmp_chan_send(lc, LMP_SEND_FLAGS_DEFAULT, *cap, 4,
                        lm[0], lm[1], lm[2], lm[3]);
            if (err_is_fail(err)) {
                DEBUG_ERR(err, "what is this?\n");
            }
            sent = err == SYS_ERR_OK;
        } while (!sent);
    }
}

static errval_t aos_rpc_call_lmp(struct aos_rpc *rpc, enum aos_rpc_msg_type msg_type, va_list args)
{
    assert(rpc);
    assert(rpc->backend == AOS_RPC_LMP);
    
    errval_t err;
    
    struct lmp_chan *lc = &rpc->channel.lmp;

    struct aos_rpc_function_binding *binding = &rpc->bindings[msg_type];
    size_t n_args = binding->n_args;
    size_t n_rets = binding->n_rets;
    void* retptrs[8];

    uintptr_t words[LMP_MSG_LENGTH];
    struct capref send_cap = NULL_CAP;
    size_t buf_page_offset = 0;
    
    words[0] = msg_type;
    int word_ind = 1;
    int ret_ind = 0;

    for (int i = 0; i < n_args; i++) {
        switch(binding->args[i]) {
            case AOS_RPC_WORD: {
                uintptr_t word = va_arg(args, uintptr_t);
                push_word_lmp(lc, words, &send_cap, &word_ind, word);
            }
            break;
            case AOS_RPC_CAPABILITY: {
                struct capref cap = va_arg(args, struct capref);
                push_cap_lmp(lc, words, &send_cap, &word_ind, cap);
            }
            break;
            case AOS_RPC_VARSTR: {
                const char *str = va_arg(args, const char *);
                uintptr_t length = strlen(str) + 1;
                push_word_lmp(lc, words, &send_cap, &word_ind, length);

                for (int j = 0; j < length; j += sizeof(uintptr_t)) {
                    uintptr_t word = 0;
                    memcpy(&word, str + j, sizeof(uintptr_t));
                    push_word_lmp(lc, words, &send_cap, &word_ind, word); // TODO: add check so we don't read over string end
                }
            }
            break;
            case AOS_RPC_STR: {
                const char* str = va_arg(args, const char*);
                if (binding->buf_page == NULL) {
                    // TODO: implement size check for string
                    err = setup_buf_page(rpc, msg_type, BASE_PAGE_SIZE * 4);
                    ON_ERR_PUSH_RETURN(err, LIB_ERR_FRAME_ALLOC); // Todo: create new error code
                }
                // send the offset into the buf_page
                push_word_lmp(lc, words, &send_cap, &word_ind, buf_page_offset);
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
    
    send_remaining_lmp(lc, words, &send_cap, &word_ind);

    for (int i = 0; i < n_rets; i++) {
        retptrs[ret_ind++] = va_arg(args, void*);
    }


    //debug_printf("waiting for response\n");
    while(!lmp_chan_can_recv(&rpc->channel.lmp)) {
        // yield to dispatcher we are communicating with
        //debug_printf("waiting for response\n");
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


static errval_t aos_rpc_unmarshall_lmp_aarch64(struct aos_rpc *rpc,
                        void *handler, struct aos_rpc_function_binding *binding,
                        struct lmp_recv_msg *msg, struct capref cap)
{
    //debug_printf("words: %ld %ld %ld %ld\n", msg->words[0], msg->words[1], msg->words[2], msg->words[3]);

    struct lmp_chan *lc = &rpc->channel.lmp;
    
    typedef uintptr_t ui;
    ui arg[7] = { 0 }; // Upper bound so only writes in register
    ui ret[4] = { 0 };
    
    char argstring[1024]; // TODO maybe move that into some scratch area in the rpc struct
    char retstring[1024];
    
    struct capref retcap = NULL_CAP;
    errval_t (*h7)(struct aos_rpc*, ui, ui, ui, ui, ui, ui, ui) = handler;
    int a_pos = 0;
    int word_ind = 1;
    int ret_pos = 0;
    for (int i = 0; i < binding->n_args; i++) {
        switch (binding->args[i]) {
            case AOS_RPC_WORD: {
                arg[a_pos++] = pull_word_lmp(lc, msg, &cap, &word_ind);
            }
            break;

            case AOS_RPC_CAPABILITY: {
                struct capref recv_cap = pull_cap_lmp(lc, msg, &cap, &word_ind);
                arg[a_pos++] = ((ui) recv_cap.cnode.croot) | (((ui) recv_cap.cnode.cnode) << 32);
                arg[a_pos++] = ((ui) recv_cap.cnode.level) | (((ui) recv_cap.slot) << 32);
            }
            break;

            case AOS_RPC_STR: {
                arg[a_pos++] = ((ui) binding->buf_page_remote) + pull_word_lmp(lc, msg, &cap, &word_ind);
            }
            break;

            case AOS_RPC_VARSTR: {
                size_t length = pull_word_lmp(lc, msg, &cap, &word_ind);
                debug_printf("reading str arg %ld\n", length);
                assert(length < sizeof argstring);
                for (size_t j = 0; j < length; j += sizeof(uintptr_t)) {
                    uintptr_t piece = pull_word_lmp(lc, msg, &cap, &word_ind);
                    memcpy(argstring + j, &piece, sizeof(uintptr_t));
                }
                arg[a_pos++] = (ui) &argstring;
            }
            break;

            default:
            debug_printf("unknown argument\n");
            break;
        }
        if (a_pos >= 7) {
            return LIB_ERR_SLAB_ALLOC_FAIL; // todo
        }
    }
    for (int i = 0; i < binding->n_rets; i++) {
        switch (binding->rets[i]) {
            case AOS_RPC_WORD: {
                arg[a_pos++] = (ui) &ret[ret_pos++];
            }
            break;

            case AOS_RPC_CAPABILITY: {
                arg[a_pos++] = (ui) &retcap;
            }
            break;

            case AOS_RPC_VARSTR: {
                arg[a_pos++] = (ui) &retstring;
                // TODO err check if used twice
            }
            break;
            
            default:
            debug_printf("response type not handled\n");
            break;
        }
        if (a_pos >= 7) {
            return LIB_ERR_SLAB_ALLOC_FAIL; // todo
        }
    }

    h7(rpc, arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6]);
    
    uintptr_t response[LMP_MSG_LENGTH];
    response[0] = binding->port | AOS_RPC_RETURN_BIT;
    struct capref response_cap = NULL_CAP;
    int buf_pos = 1;
    ret_pos = 0;

    for (int i = 0; i < binding->n_rets; i++) {
        switch(binding->rets[i]) {
            case AOS_RPC_WORD: {
                uintptr_t word = ret[ret_pos++];
                push_word_lmp(lc, response, &response_cap, &buf_pos, word);
            }
            break;
            
            case AOS_RPC_CAPABILITY: {
                push_cap_lmp(lc, response, &response_cap, &buf_pos, retcap);
            }
            break;

            case AOS_RPC_VARSTR: {
                uintptr_t length = strlen(retstring) + 1;
                push_word_lmp(lc, response, &response_cap, &buf_pos, length);
                for (int j = 0; j < length; j += sizeof(uintptr_t)) {
                    uintptr_t word;
                    memcpy(&word, retstring + j, sizeof(uintptr_t));
                    push_word_lmp(lc, response, &response_cap, &buf_pos, word);
                }
            }
            break;

            default:
            debug_printf("unhandled ret arg\n");
            break;
        }
    }
    send_remaining_lmp(lc, response, &response_cap, &buf_pos);
    

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
    else if (err == LIB_ERR_NO_LMP_MSG) {
        err = lmp_chan_register_recv(channel, get_default_waitset(), MKCLOSURE(&aos_rpc_on_message, arg));
        return;
    }
    else if (err_is_fail(err)) {
        debug_printf("error is: %ld\n", err);
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


    err = aos_rpc_unmarshall_lmp_aarch64(rpc, handler, binding, &msg, recieved_cap);

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


static uintptr_t pull_word(struct ump_chan *uc, struct ump_msg *um, int *word_ind)
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


static errval_t aos_rpc_unmarshall_ump_simple_aarch64(struct aos_rpc *rpc,
                        void *handler, struct aos_rpc_function_binding *binding,
                        struct ump_msg *msg)
{
    /*debug_printf("words: %ld %ld %ld %ld %ld %ld %ld\n",
        msg->data[0], msg->data[1], msg->data[2], msg->data[3],
        msg->data[4], msg->data[5], msg->data[6]
    );*/
    
    struct ump_chan *uc = &rpc->channel.ump;

    typedef uintptr_t ui;
    ui arg[8] = { 0 }; // Upper bound so only writes in register
    ui ret[8] = { 0 };
    errval_t (*h8)(struct aos_rpc*, ui, ui, ui, ui, ui, ui, ui, ui) = handler;
    
    // index into msg
    int word_ind = 1;

    int a_pos = 0;
    int ret_pos = 0;
    struct capref retcap;
    char argstring[4096]; // very dangerous
    char retstring[4096]; // very dangerous
    for (int i = 0; i < binding->n_args; i++) {
        
        switch(binding->args[i]) {
            case AOS_RPC_WORD: {
                arg[a_pos++] = pull_word(uc, msg, &word_ind);
            }
            break;
            case AOS_RPC_CAPABILITY: {
                uintptr_t words[3];
                words[0] = pull_word(uc, msg, &word_ind);
                words[1] = pull_word(uc, msg, &word_ind);
                words[2] = pull_word(uc, msg, &word_ind);

                struct capability cap;
                memcpy(&cap, words, sizeof cap);

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
            break;
            case AOS_RPC_VARSTR: {
                uintptr_t length = pull_word(uc, msg, &word_ind);
                debug_printf("here on arg %d, length: %ld\n", i, length);
                assert(length < sizeof argstring);
                for (size_t j = 0; j < length; j += sizeof(uintptr_t)) {
                    uintptr_t piece = pull_word(uc, msg, &word_ind);
                    debug_printf("piece: %ld\n", piece);
                    memcpy(argstring + j, &piece, sizeof(uintptr_t));
                }
                debug_printf("argstring: %s\n", argstring);
                arg[a_pos++] = (ui) &argstring;
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
                arg[a_pos++] = (ui) &ret[ret_pos++];
            }
            break;
            case AOS_RPC_SHORTSTR: {
                arg[a_pos++] = (ui) &ret[ret_pos];
                ret_pos += 4;
            }
            break;
            case AOS_RPC_CAPABILITY: {
                if (retcap_used) return LIB_ERR_NOT_IMPLEMENTED; // error, sending multiple caps nyi
                arg[a_pos++] = (ui) &retcap;
                retcap_used = true;
            }
            break;
            case AOS_RPC_STR: {
                arg[a_pos++] = (ui) &retstring; // todo replace with scratch space
            }
            break;

            default:
            debug_printf("unhandled ret arg\n");
            break;
        }

        if (a_pos >= 7) {
            return LIB_ERR_SLAB_ALLOC_FAIL; // todo
        }
    }

    h8(rpc, arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6], arg[7]);

    // send response
    DECLARE_MESSAGE(rpc->channel.ump, response);
    response->flag = 0;
    response->data[0] = msg->data[0] | AOS_RPC_RETURN_BIT;

    int buf_pos = 1;
    ret_pos = 0;
    for (int i = 0; i < binding->n_rets; i++) {
        switch(binding->rets[i]) {
            case AOS_RPC_WORD: {
                uintptr_t word = ret[ret_pos++];
                push_words(uc, response, &buf_pos, word);
            }
            break;
            
            case AOS_RPC_CAPABILITY: {
                struct capability cap;
                errval_t err = invoke_cap_identify(retcap, &cap);
                ON_ERR_PUSH_RETURN(err, LIB_ERR_CAP_IDENTIFY);
                uintptr_t words[3];
                memcpy(&words, &cap, sizeof cap);
                push_words(uc, response, &buf_pos, words[0]);
                push_words(uc, response, &buf_pos, words[1]);
                push_words(uc, response, &buf_pos, words[2]);
            }
            break;

            case AOS_RPC_VARSTR: {
                uintptr_t length = strlen(retstring);
                push_words(uc, response, &buf_pos, length);
                
                for (int j = 0; j < length; j += sizeof(uintptr_t)) {
                    uintptr_t word;
                    memcpy(&word, retstring + j, sizeof(uintptr_t));
                    push_words(uc, response, &buf_pos, word);
                }
            }
            break;

            default:
            debug_printf("unhandled ret arg\n");
            break;
        }
    }

    send_remaining(uc, response, &buf_pos);

    return SYS_ERR_OK;
}

void aos_rpc_on_ump_message(void *arg)
{
    debug_printf("ump message received!\n");
    errval_t err;

    struct aos_rpc *rpc = arg;
    DECLARE_MESSAGE(rpc->channel.ump, msg);
    msg->flag = 0;

    bool received = ump_chan_poll_once(&rpc->channel.ump, msg);
    if (!received) {
        debug_printf("aos_rpc_on_ump_message called but no message available\n");
        ump_chan_register_recv(&rpc->channel.ump, get_default_waitset(), MKCLOSURE(&aos_rpc_on_ump_message, rpc));
        return;
    }

    enum aos_rpc_msg_type msgtype = msg->data[0];

    void *handler = rpc->handlers[msgtype];
    if (handler == NULL) {
        debug_printf("no handler for %d\n", msgtype);
        ump_chan_register_recv(&rpc->channel.ump, get_default_waitset(), MKCLOSURE(&aos_rpc_on_ump_message, rpc));
        return;
    }

    struct aos_rpc_function_binding *binding = &rpc->bindings[msgtype];

    err = aos_rpc_unmarshall_ump_simple_aarch64(rpc, handler, binding, msg);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "error in unmarshall\n");
    }

    ump_chan_register_recv(&rpc->channel.ump, get_default_waitset(), MKCLOSURE(&aos_rpc_on_ump_message, rpc));

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
