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
#include <arch/aarch64/aos/lmp_chan_arch.h>
#include <stdarg.h>

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
    err = frame_alloc(&frame, size, NULL);
    //err = aos_rpc_get_ram_cap(rpc, BASE_PAGE_SIZE, 1, &frame, NULL);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_FRAME_ALLOC);

    char buf[128];
    debug_print_cap_at_capref(buf, sizeof buf, frame);
    debug_printf("frame: %s\n", buf);
    
    err = paging_map_frame_complete(get_current_paging_state(), &rpc->bindings[msg_type].buf_page, frame, NULL, NULL);
    debug_printf("new buf_ptr %p\n", &rpc->bindings[msg_type].buf_page);
    DEBUG_ERR(err, "asasasa");
    ON_ERR_PUSH_RETURN(err, LIB_ERR_VSPACE_MAP);

    debug_printf("seting up the buffer!\n");
    err = aos_rpc_call(rpc, AOS_RPC_SEND_NUMBER, 12345);
    err = aos_rpc_call(rpc, AOS_RPC_SEND_NUMBER, 54321);
    err = aos_rpc_call(rpc, AOS_RPC_SETUP_PAGE, msg_type, size, frame);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_GET_MEM_IREF); // TODO: New error code
    
    return SYS_ERR_OK;
}

/**
 * NOTE: maybe add remote and local cap as parameters?
 *       atm they have to be set before calling dis method
 * i.e. rpc->channel.local_cap / remote_cap are used here
 * and should be properly set beforehand
 * \brief Initialize an aos_rpc struct.
 */
errval_t aos_rpc_init(struct aos_rpc* rpc, struct capref self_ep, struct capref end_ep, struct lmp_endpoint *lmp_ep) {
    errval_t err;
    debug_printf("aos_rpc_init\n");

    lmp_chan_init(&rpc->channel);
    rpc->channel.local_cap = self_ep;
    rpc->channel.remote_cap = end_ep;

    memset(rpc->bindings, 0, sizeof rpc->bindings);
    if (lmp_ep == NULL) {
        err = endpoint_create(256, &rpc->channel.local_cap, &lmp_ep);
        ON_ERR_PUSH_RETURN(err, LIB_ERR_ENDPOINT_CREATE);
    }

    rpc->channel.buflen_words = 256;
    rpc->channel.endpoint = lmp_ep;
    lmp_chan_alloc_recv_slot(&rpc->channel);

    // bind initiate function (to send our endpoint to init)
    aos_rpc_initialize_binding(rpc, AOS_RPC_INITIATE, 1, 0, AOS_RPC_CAPABILITY);

    // bind further functions
    aos_rpc_initialize_binding(rpc, AOS_RPC_SEND_NUMBER, 1, 0, AOS_RPC_WORD);
    aos_rpc_initialize_binding(rpc, AOS_RPC_SEND_STRING, 1, 0, AOS_RPC_STR);
    aos_rpc_initialize_binding(rpc, AOS_RPC_REQUEST_RAM, 2, 2, AOS_RPC_WORD, AOS_RPC_WORD, AOS_RPC_CAPABILITY, AOS_RPC_WORD);

    aos_rpc_initialize_binding(rpc, AOS_RPC_SETUP_PAGE, 3, 0, AOS_RPC_WORD, AOS_RPC_WORD, AOS_RPC_CAPABILITY);
    aos_rpc_register_handler(rpc, AOS_RPC_SETUP_PAGE, &aos_rpc_setup_page_handler);

    aos_rpc_initialize_binding(rpc, AOS_RPC_PROC_SPAWN_REQUEST, 2, 1, AOS_RPC_STR, AOS_RPC_WORD, AOS_RPC_WORD);

    err = lmp_chan_register_recv(&rpc->channel, get_default_waitset(), MKCLOSURE(&aos_rpc_on_message, rpc));

    if (!capref_is_null(rpc->channel.remote_cap)) {
        // we are not in init
        debug_printf("Trying to establish channel with init:\n");

        // call initiate function with our endpoint cap as argument in order
        // to make it known to init
        err = aos_rpc_call(rpc, AOS_RPC_INITIATE, rpc->channel.local_cap);
        ON_ERR_PUSH_RETURN(err, LIB_ERR_MONITOR_RPC_BIND);


    }

    return err;
}


errval_t
aos_rpc_send_number(struct aos_rpc *rpc, uintptr_t num) {
    return aos_rpc_call(rpc, AOS_RPC_SEND_NUMBER, num);
}




errval_t
aos_rpc_send_string(struct aos_rpc *rpc, const char *string) {
    aos_rpc_call(rpc, AOS_RPC_SEND_STRING, string);
return SYS_ERR_OK;
    /*errval_t err = SYS_ERR_OK;
    size_t size = 0;
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;

    for(;true;++size){if(string[size] == '\0'){break;}}
    debug_printf("Sending string of size: %d\n",size);
    err = lmp_chan_send2(&rpc -> channel, LMP_SEND_FLAGS_DEFAULT,NULL_CAP,AOS_RPC_SEND_STRING,size);
    if(err_is_fail(err)){
      DEBUG_ERR(err,"Failed to send number");
    }
    bool can_receive = lmp_chan_can_recv(&rpc->channel);
    while(!can_receive){
      can_receive = lmp_chan_can_recv(&rpc->channel);
    }

    err = lmp_chan_recv(&rpc -> channel, &msg, &NULL_CAP);
    if(err_is_fail(err) || msg.words[0] != AOS_RPC_ACK){
      debug_printf("First word should be ACK, is: %d\n",msg.words[0]);
      DEBUG_ERR(err,"Could not get receive for sent string size");
    }


    size_t i = 0;
    // rpc -> channel.endpoint -> k.delivered = false;
    while(i < size){
      // debug_printf("Sending string at %c\n",string[i]);
      err = lmp_chan_send1(&rpc -> channel, LMP_SEND_FLAGS_DEFAULT,NULL_CAP,string[i]);
      if(err_is_fail(err)){
        DEBUG_ERR(err,"Failed to send string character at: %d",i);
      }
      can_receive = lmp_chan_can_recv(&rpc->channel);
      while(!can_receive){
        can_receive = lmp_chan_can_recv(&rpc->channel);
      }
      err = lmp_chan_recv(&rpc -> channel, &msg, &NULL_CAP);
      if(err_is_fail(err) || msg.words[0] != AOS_RPC_ACK){
        debug_printf("First word should be ACK, is: %d\n",msg.words[0]);
        DEBUG_ERR(err,"Could not get receive for sent string at i:%\n",i);
      }

      i++;
    }
    return err;*/
}



errval_t
aos_rpc_get_ram_cap(struct aos_rpc *rpc, size_t bytes, size_t alignment,
                    struct capref *ret_cap, size_t *ret_bytes) {
    size_t _rs = 0;
    return aos_rpc_call(rpc, AOS_RPC_REQUEST_RAM, bytes, alignment, ret_cap, ret_bytes ? : &_rs);
    
    // TODO: implement functionality to request a RAM capability over the
    // given channel and wait until it is delivered.
    /*errval_t err;

    err = lmp_chan_alloc_recv_slot(&rpc->channel);
    ON_ERR_RETURN(err);

    debug_printf("senmding request\n");
    err = lmp_chan_send3(&rpc->channel, LMP_SEND_FLAGS_DEFAULT, NULL_CAP, AOS_RPC_REQUEST_RAM, bytes, alignment);
    if(err_is_fail(err)) {
        DEBUG_ERR(err, "failed to call lmp_chan_send");
    }

    debug_printf("recieving answqer\n");
    bool can_receive = lmp_chan_can_recv(&rpc->channel);
    while(!can_receive) {
        can_receive = lmp_chan_can_recv(&rpc->channel);
    }
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    err = lmp_chan_recv(&rpc->channel, &msg, ret_cap);

    if(err_is_fail(err) || msg.words[0] != AOS_RPC_RAM_SEND){
        DEBUG_ERR(err, "did not receive ram cap");
        return err;
    }

    return SYS_ERR_OK;*/
}


errval_t
aos_rpc_serial_getchar(struct aos_rpc *rpc, char *retc) {
    // TODO implement functionality to request a character from
    // the serial driver.
    return SYS_ERR_OK;
}


errval_t
aos_rpc_serial_putchar(struct aos_rpc *rpc, char c) {
    // TODO implement functionality to send a character to the
    // serial port.
    return SYS_ERR_OK;
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
            *((uintptr_t*)retptrs[i]) = msg->buf.words[msg_ind];
        }
        else if (binding->rets[i] == AOS_RPC_CAPABILITY) {
            *((struct capref*) retptrs[i]) = cap;
        }
    }
    return SYS_ERR_OK;
}


errval_t aos_rpc_call(struct aos_rpc *rpc, enum aos_rpc_msg_type msg_type, ...)
{
    va_list args;

    errval_t err;

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
        lmp_chan_send(&rpc->channel, LMP_SEND_FLAGS_DEFAULT, cap, n_args + 1,
                    msg_type, words[0], words[1], words[2]);
    }
    else {
        debug_printf("unknown binding\n");
    }
    //debug_printf("waiting for response\n");
    bool can_receive = false;
    can_receive = lmp_chan_can_recv(&rpc->channel);
    while(!can_receive) {
        can_receive = lmp_chan_can_recv(&rpc->channel);
    }
    //debug_printf("getting response\n");

    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref recieved_cap = NULL_CAP;

    err = lmp_chan_recv(&rpc->channel, &msg, &recieved_cap);
    ON_ERR_RETURN(err);
    //debug_printf("got response\n");

    if (!capref_is_null(recieved_cap)) {
        lmp_chan_alloc_recv_slot(&rpc->channel);
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
    debug_printf("words: %ld %ld %ld %ld\n", msg->buf.words[0], msg->buf.words[1], msg->buf.words[2], msg->buf.words[3]);

    typedef uintptr_t ui;
    ui arg[8] = { 0 };
    ui ret[4] = { 0 };
    struct capref retcap = NULL_CAP;
    errval_t (*h4)(struct aos_rpc*, ui, ui, ui, ui) = handler;
    errval_t (*h7)(struct aos_rpc*, ui, ui, ui, ui, ui, ui, ui, ui) = handler;
    int a_pos = 0;
    int buf_pos = 1;
    int ret_pos = 0;
    for (int i = 0; i < binding->n_args; i++) {
        if (binding->args[i] == AOS_RPC_WORD) {
            arg[a_pos++] = msg->buf.words[buf_pos++];
        }
        else if (binding->args[i] == AOS_RPC_CAPABILITY) {
            arg[a_pos++] = (cap.cnode.croot) | (((ui)cap.cnode.cnode) << 32);
            arg[a_pos++] = (cap.cnode.level) | (((ui)cap.slot) << 32);
        }
        else if (binding->args[i] == AOS_RPC_STR) {
            arg[a_pos++] = ((ui) binding->buf_page_remote) + msg->buf.words[buf_pos++];
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
        else {
            // TODO: error, can't transmit strings or other over simple channels
        }
        if (a_pos >= 9) {
            return LIB_ERR_SLAB_ALLOC_FAIL; // todo
        }
    }


    //debug_printf("calling handler for %d: %ld, %ld, %ld, %ld\n", binding->port, arg[0], arg[1], arg[2], arg[3]);
    if (a_pos == 4) {
        h4(rpc, arg[0], arg[1], arg[2], arg[3]);
    }
    else {
        h7(rpc, arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6], arg[7]);
    }
    //char buf[128];
    //debug_print_cap_at_capref(buf, sizeof buf, retcap);
    //debug_printf("retcap: %s\n", buf);

    // send response
    lmp_chan_send(&rpc->channel, LMP_SEND_FLAGS_DEFAULT, retcap, ret_pos + 1, binding->port | AOS_RPC_RETURN_BIT, ret[0], ret[1], ret[2]);

    return SYS_ERR_OK;
}

extern long vaerboes;
long vaerboes = 0;
void aos_rpc_test(uintptr_t x, uintptr_t y, uintptr_t z, uintptr_t w, uintptr_t c, uintptr_t a, uintptr_t b, uintptr_t d)
{
    vaerboes = b;
}

void aos_rpc_on_message(void *arg)
{
    //debug_printf("aos_rpc_on_message\n");
    struct aos_rpc *rpc = arg;
    struct lmp_chan *channel = &rpc->channel;

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

    uintptr_t msgtype = msg.buf.words[0];

    bool is_response = false;
    if (msgtype & AOS_RPC_RETURN_BIT) {
        msgtype &= ~AOS_RPC_RETURN_BIT;
        is_response = true;
    }

    // hardcode initial "handshake"
    /*if (msgtype == AOS_RPC_INITIATE) {
        rpc->channel.remote_cap = recieved_cap;
        lmp_chan_send1(&rpc->channel, LMP_SEND_FLAGS_DEFAULT, NULL_CAP, AOS_RPC_ACK);
        debug_printf("sent handshake response\n");
        goto on_success;
    }*/

    void *handler = rpc->handlers[msgtype];
    if (handler == NULL) {
        debug_printf("msgtype: %d no handler set\n");
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

/**
 * \brief Returns the RPC channel to init.
 */
struct aos_rpc *aos_rpc_get_init_channel(void)
{
    return get_init_rpc();
/*
    struct capref self_ep_cap = (struct capref){
      .cnode = cnode_task,
      .slot = TASKCN_SLOT_SELFEP
    };

    struct capref init_ep_cap = (struct capref){
      .cnode = cnode_task,
      .slot = TASKCN_SLOT_INITEP
    };
    lmp_endpoint_init();

    // create and initialize rpc
    struct aos_rpc* rpc = (struct aos_rpc *) malloc(sizeof(struct aos_rpc));
    rpc -> channel.local_cap = self_ep_cap; // set required struct members
    rpc -> channel.remote_cap = init_ep_cap;
    aos_rpc_init(rpc); // init rpc

    return rpc;*/
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
    return NULL;
}
