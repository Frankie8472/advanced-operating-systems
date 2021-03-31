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



/**
 * NOTE: maybe add remote and local cap as parameters?
 *       atm they have to be set before calling dis method
 * i.e. rpc->channel.local_cap / remote_cap are used here
 * and should be properly set beforehand
 * \brief Initialize an aos_rpc struct.
 */
errval_t aos_rpc_init(struct aos_rpc* rpc) {

    lmp_chan_init(&rpc->channel);

    struct lmp_endpoint *lmp_ep;
    errval_t err = endpoint_create(16, &rpc->channel.local_cap, &lmp_ep);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_ENDPOINT_CREATE);

    rpc->channel.endpoint = lmp_ep;


    // bind initiate function (to send our endpoint to init)
    aos_rpc_initialize_binding(rpc, AOS_RPC_INITIATE, 1, AOS_RPC_CAPABILITY);

    // bind further functions
    aos_rpc_initialize_binding(rpc, AOS_RPC_SEND_NUMBER, AOS_RPC_NO_TYPE, 1, AOS_RPC_WORD);
    aos_rpc_initialize_binding(rpc, AOS_RPC_SEND_STRING, AOS_RPC_NO_TYPE, 1, AOS_RPC_STR);
    aos_rpc_initialize_binding(rpc, AOS_RPC_REQUEST_RAM, AOS_RPC_CAPABILITY, 2, AOS_RPC_WORD, AOS_RPC_WORD);

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
    errval_t err = SYS_ERR_OK;

    bool can_receive = lmp_chan_can_recv(&rpc->channel);
    // debug_printf("Curr can_receive: %d\n",can_receive);

    // debug_printf("Waiting to receive ACK\n");
    err = lmp_chan_send2(&rpc -> channel, LMP_SEND_FLAGS_DEFAULT,NULL_CAP,AOS_RPC_SEND_NUMBER,num);
    if(err_is_fail(err)){
      DEBUG_ERR(err,"failed to send number");
    }
    while(!can_receive){
      can_receive = lmp_chan_can_recv(&rpc->channel);
    }
    // debug_printf("can_receive after send: %d\n",can_receive);

    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    err = lmp_chan_recv(&rpc -> channel, &msg, &NULL_CAP);
    if(err_is_fail(err) || msg.words[0] != AOS_RPC_ACK){
      debug_printf("First word should be ACK, is: %d\n",msg.words[0]);
      DEBUG_ERR(err,"Could not get receive for sent number");
    }
    debug_printf("Acks for: %d has been received!\n",num);
    return err;
}




errval_t
aos_rpc_send_string(struct aos_rpc *rpc, const char *string) {

    errval_t err = SYS_ERR_OK;
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
    return err;
}



errval_t
aos_rpc_get_ram_cap(struct aos_rpc *rpc, size_t bytes, size_t alignment,
                    struct capref *ret_cap, size_t *ret_bytes) {
    // TODO: implement functionality to request a RAM capability over the
    // given channel and wait until it is delivered.
    errval_t err;

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

    return SYS_ERR_OK;
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
    errval_t err;

    err = lmp_chan_alloc_recv_slot(&rpc->channel);
    ON_ERR_RETURN(err);
    size_t len = strlen(cmdline);
    err = lmp_chan_send3(&rpc->channel, LMP_SEND_FLAGS_DEFAULT, NULL_CAP, AOS_RPC_PROC_SPAWN_REQUEST, len, core);

    for (int i = 0; i < len; i++) {
        err = lmp_chan_send1(&rpc->channel, LMP_SEND_FLAGS_DEFAULT, NULL_CAP, cmdline[i]);
        if(err_is_fail(err)) {
            DEBUG_ERR(err, "failed to call lmp_chan_send");
        }
    }

    bool can_receive = lmp_chan_can_recv(&rpc->channel);
    while(!can_receive){
        can_receive = lmp_chan_can_recv(&rpc->channel);
    }

    struct lmp_recv_msg msg_string = LMP_RECV_MSG_INIT;
    err = lmp_chan_recv(&rpc->channel, &msg_string, &NULL_CAP);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"Could not receive string\n");
    }

    *newpid = msg_string.words[0];

    return SYS_ERR_OK;
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
                        void *retval, struct aos_rpc_function_binding *binding,
                        struct lmp_recv_msg *msg, struct capref cap)
{
    /*if (binding->return_type == AOS_RPC_CAPABILITY) {
        *((struct capref*) retval) = cap;
    }
    else if (binding->return_type == AOS_RPC_WORD) {
        *((uintptr_t*) retval) = msg->buf.words[1];
    }*/

    return SYS_ERR_OK;
}


errval_t aos_rpc_call(struct aos_rpc *rpc, enum aos_rpc_msg_type msg_type, ...)
{
    va_list args;

    struct aos_rpc_function_binding *binding = &rpc->bindings[msg_type];
    size_t n_args = binding->n_args;
    if (binding->calling_simple) {
        assert(n_args < LMP_MSG_LENGTH);
        uintptr_t words[LMP_MSG_LENGTH];
        va_start(args, msg_type);
        for (int i = 0; i < n_args; i++) {
            words[i] = va_arg(args, uintptr_t);
        }
        va_end(args);
        lmp_chan_send(&rpc->channel, LMP_SEND_FLAGS_DEFAULT, rpc->channel.local_cap, n_args,
                    binding->port, words[0], words[1], words[2]);
    }
    else {
        debug_printf("unknown binding\n");
    }

    bool can_receive = false;
    can_receive = lmp_chan_can_recv(&rpc->channel);
    while(!can_receive) {
        can_receive = lmp_chan_can_recv(&rpc->channel);
    }

    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref recieved_cap = NULL_CAP;
    errval_t err = lmp_chan_recv(&rpc->channel, &msg, &recieved_cap);
    ON_ERR_RETURN(err);

    err = aos_rpc_unmarshall_retval_aarch64(rpc, binding, &msg, recieved_cap);
    
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
    fb->port = binding;


    int wordargs = 0;
    int strargs = 0;
    int capargs = 0;

    int wordrets = 0;
    int strrets = 0;
    int caprets = 0;

    va_start(args, n_rets);
    for (int i = 0; i < n_args; i++) {
        fb->args[i] = va_arg(args, int); // read enum values promoted to int
        if (fb->args[i] == AOS_RPC_WORD) wordargs++;
        if (fb->args[i] == AOS_RPC_STR) strargs++;
        if (fb->args[i] == AOS_RPC_CAPABILITY) capargs++;
    }
    for (int i = 0; i < n_rets; i++) {
        fb->rets[i] = va_arg(args, int); // read enum values promoted to int
        if (fb->args[i] == AOS_RPC_WORD) wordrets++;
        if (fb->args[i] == AOS_RPC_STR) strrets++;
        if (fb->args[i] == AOS_RPC_CAPABILITY) caprets++;
    }
    va_end(args);

    fb->calling_simple = wordargs <= 3 && strargs == 0 && capargs <= 1;
    fb->returning_simple = wordrets <= 3 && strrets == 0 && caprets <= 1;

    return SYS_ERR_OK;
}


static errval_t aos_rpc_unmarshall_simple_aarch64(struct aos_rpc *rpc,
                        void *handler, struct aos_rpc_function_binding *binding,
                        struct lmp_recv_msg *msg, struct capref cap)
{
    if (binding->args[0] == AOS_RPC_CAPABILITY && binding->n_args == 1) {
        uintptr_t (*cap_handler)(struct capref) = handler;
        uintptr_t result = cap_handler(cap);
        result += 12345;
    }
    else {
        uintptr_t (*int_handler)(uintptr_t, uintptr_t, uintptr_t) = handler;
        uintptr_t result = int_handler(msg->buf.words[1], msg->buf.words[2], msg->buf.words[3]);
        result += 12345;
    }

    if (binding->n_rets == 0) {
        lmp_chan_send1(&rpc->channel, LMP_SEND_FLAGS_DEFAULT, NULL_CAP, binding->port | AOS_RPC_RETURN_MASK);
    }

    return SYS_ERR_OK;
}

extern long vaerboes;
long vaerboes = 0;
void aos_rpc_test(struct capref c, uintptr_t a , uintptr_t b)
{
    vaerboes = a;
}

void aos_rpc_on_message(void *arg)
{
    debug_printf("in aos_rpc_on_message\n");
    struct aos_rpc *rpc = arg;
    struct lmp_chan *channel = &rpc->channel;

    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    struct capref recieved_cap = NULL_CAP;
    errval_t err;
    
    err = lmp_chan_recv(channel, &msg, &recieved_cap);
    if (err_is_fail(err)) {
        err = err_push(err, LIB_ERR_LMP_CHAN_RECV);
        goto on_error;
    }

    uintptr_t msgtype = msg.buf.words[0];

    bool is_response = false;
    if (msgtype & AOS_RPC_RETURN_MASK) {
        msgtype &= ~AOS_RPC_RETURN_MASK;
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
        debug_printf("msgtype: %d\n");
        err = LIB_ERR_NOT_IMPLEMENTED;
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

    return rpc;
}

/**
 * \brief Returns the channel to the memory server
 */
struct aos_rpc *aos_rpc_get_memory_channel(void)
{
    //TODO: Return channel to talk to memory server process (or whoever
    //implements memory server functionality)
    //debug_printf("aos_rpc_get_memory_channel NYI\n");
    return aos_rpc_get_init_channel();
    return NULL;
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
