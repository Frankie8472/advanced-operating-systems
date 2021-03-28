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





errval_t
aos_rpc_send_number(struct aos_rpc *rpc, uintptr_t num) {
    // TODO: implement functionality to send a number over the channel
    // given channel and wait until the ack gets returned.

    void dosend(void* a) {
        debug_printf("dosend\n");
    }

    struct event_closure closure = MKCLOSURE(&dosend, rpc);
    struct waitset *ws = get_default_waitset();
    lmp_chan_register_send(&rpc->channel, ws, closure);
    lmp_chan_send1(&rpc->channel, LMP_SEND_FLAGS_DEFAULT, NULL_CAP, num);
    return SYS_ERR_OK;
}

errval_t
aos_rpc_send_string(struct aos_rpc *rpc, const char *string) {
    // TODO: implement functionality to send a string over the given channel
    // and wait for a response.
    return SYS_ERR_OK;
}

errval_t
aos_rpc_get_ram_cap(struct aos_rpc *rpc, size_t bytes, size_t alignment,
                    struct capref *ret_cap, size_t *ret_bytes) {
    // TODO: implement functionality to request a RAM capability over the
    // given channel and wait until it is delivered.
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



/**
 * \brief Returns the RPC channel to init.
 */
struct aos_rpc *aos_rpc_get_init_channel(void)
{
    /*static struct aos_rpc init;
    static bool initialized = false;
    if (!initialized) {
        aos_rpc_init(&init);
        init.endpoint = (struct capref) {
            .cnode = cnode_task,
            .slot = TASKCN_SLOT_INITEP
        };
        lmp_chan_init(&init.channel);
    }

    debug_printf("aos_rpc_get_init_channel\n");
    return &init;
    */
    //TODO: Return channel to talk to init process
    // debug_printf("aos_rpc_get_init_channel NYI\n");
    //
    struct capref self_ep_cap = (struct capref){
      .cnode = cnode_task,
      .slot = TASKCN_SLOT_SELFEP
    };

    struct capref init_ep_cap = (struct capref){
      .cnode = cnode_task,
      .slot = TASKCN_SLOT_INITEP
    };

    struct capref new_cap;
    struct lmp_endpoint *lmp_ep;
    errval_t err = endpoint_create(16, &new_cap, &lmp_ep);
    DEBUG_ERR(err, "err");

    //char capmsgg[512];
    //debug_print_cap_at_capref(capmsgg, sizeof capmsgg, lmp_ep->recv_slot);
    //debug_printf("lmp_ep->recv_slot is: %s\n", capmsgg);

    lmp_ep->recv_slot = init_ep_cap;
    struct aos_rpc* rpc = (struct aos_rpc *) malloc(sizeof(struct aos_rpc));
    lmp_chan_init(&rpc -> channel);
    rpc -> channel.local_cap = self_ep_cap;
    rpc -> channel.remote_cap = init_ep_cap;

    char capmsg[512];
    debug_print_cap_at_capref(capmsg, sizeof capmsg, self_ep_cap);
    debug_printf("cap is: %s\n", capmsg);


    printf("Sending word: 5\n");
    err = lmp_chan_send1(&rpc -> channel, LMP_SEND_FLAGS_DEFAULT, NULL_CAP, 5);
    if(err_is_fail(err)){
      DEBUG_ERR(err,"failed to call lmp_chan_send");
    }
    //try to send to a message to init
    // uint32_t * buffer

    return rpc;
}

/**
 * \brief Returns the channel to the memory server
 */
struct aos_rpc *aos_rpc_get_memory_channel(void)
{
    //TODO: Return channel to talk to memory server process (or whoever
    //implements memory server functionality)
    debug_printf("aos_rpc_get_memory_channel NYI\n");
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
