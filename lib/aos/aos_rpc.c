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


// __attribute__ ((unused)) static void receive_handler(void *arg){
//
// }


errval_t
aos_rpc_send_number(struct aos_rpc *rpc, uintptr_t num) {
    errval_t err = SYS_ERR_OK;

    bool can_receive = lmp_chan_can_recv(&rpc->channel);
    // debug_printf("Curr can_receive: %d\n",can_receive);

    // debug_printf("Waiting to receive ACK\n");
    err = lmp_chan_send2(&rpc -> channel, LMP_SEND_FLAGS_DEFAULT,NULL_CAP,AOS_RPC_NUMBER,num);
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
    err = lmp_chan_send2(&rpc -> channel, LMP_SEND_FLAGS_DEFAULT,NULL_CAP,AOS_RPC_STRING,size);
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

    err = lmp_chan_send3(&rpc->channel, LMP_SEND_FLAGS_DEFAULT, NULL_CAP, AOS_RPC_RAM_REQUEST, bytes, alignment);
    if(err_is_fail(err)) {
        DEBUG_ERR(err, "failed to call lmp_chan_send");
    }

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
    errval_t err = SYS_ERR_OK;
    if (!retc) { // if retcap NULL was given, just return OK
        return err;
    }

    err = lmp_chan_send1(&rpc -> channel, LMP_SEND_FLAGS_DEFAULT,NULL_CAP,AOS_RPC_GETCHAR);
    if (err_is_fail(err)) {
        DEBUG_ERR(err,"Failed to send character to serial port");
    }

    while (!lmp_chan_can_recv(&rpc->channel))
        ;

    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    err = lmp_chan_recv(&rpc->channel, &msg, &NULL_CAP);

    if (err_is_fail(err) || msg.words[0] != AOS_RPC_STRING) {
        DEBUG_ERR(err, "getchar did not receive a string as response");
        return err;
    }

    *retc = msg.words[1];

    return err;
}

errval_t aos_rpc_get_terminal_input(struct aos_rpc *rpc, char* buf,size_t len){
  errval_t err = SYS_ERR_OK;



  err = lmp_chan_send1(&rpc -> channel, LMP_SEND_FLAGS_DEFAULT,NULL_CAP,AOS_RPC_SET_READ);
  if (err_is_fail(err)) {
      DEBUG_ERR(err,"Failed to send RPC get read to init");
  }

  while (!lmp_chan_can_recv(&rpc->channel))
      ;

  struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
  err = lmp_chan_recv(&rpc->channel, &msg, &NULL_CAP);

  if (err_is_fail(err) || msg.words[0] != AOS_RPC_ACK) {
      DEBUG_ERR(err, "getchar did not receive a ack for read lock");
      return err;
  }


  int i = 0;
  while(i < len -1){
    aos_rpc_serial_getchar(rpc,&buf[i]);
    if(buf[i] == 13){break;}
    ++i;
  }
  buf[i + 1] = '\0';


  err = lmp_chan_send1(&rpc -> channel, LMP_SEND_FLAGS_DEFAULT,NULL_CAP,AOS_RPC_FREE_READ);
  if (err_is_fail(err)) {
      DEBUG_ERR(err,"Failed to send RPC free read to init");
  }

  while (!lmp_chan_can_recv(&rpc->channel))
      ;

  // msg = LMP_RECV_MSG_INIT;
  err = lmp_chan_recv(&rpc->channel, &msg, &NULL_CAP);

  if (err_is_fail(err) || msg.words[0] != AOS_RPC_ACK) {
      DEBUG_ERR(err, "getchar did not receive ack for read free");
      return err;
  }

  return err;
}


errval_t
aos_rpc_serial_putchar(struct aos_rpc *rpc, char c) {

    errval_t err = SYS_ERR_OK;
    err = lmp_chan_send2(&rpc -> channel, LMP_SEND_FLAGS_DEFAULT,NULL_CAP,AOS_RPC_PUTCHAR,c);
    if(err_is_fail(err)){
      DEBUG_ERR(err,"Failed to send character to serial port");
    }
    return err;
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


/**
 * NOTE: maybe add remote and local cap as parameters?
 *       atm they have to be set before calling dis method
 * i.e. rpc->channel.local_cap / remote_cap are used here
 * and should be properly set beforehand
 * \brief Initialize an aos_rpc struct.
 */
errval_t aos_rpc_init(struct aos_rpc* rpc) {
    struct lmp_endpoint *lmp_ep;
    errval_t err = endpoint_create(16, &rpc->channel.local_cap, &lmp_ep);
    if (err_is_fail(err)) {
        DEBUG_ERR(err, "err");
        return err;
    }

    lmp_ep->recv_slot = rpc -> channel.remote_cap;
    lmp_chan_init(&rpc -> channel);
    rpc -> channel.endpoint = lmp_ep;
    debug_printf("Trying to establish channel with init:\n");

    err = lmp_chan_send1(&rpc -> channel, LMP_SEND_FLAGS_DEFAULT, rpc->channel.local_cap, AOS_RPC_INIT);
    if(err_is_fail(err)){
        DEBUG_ERR(err,"failed to call lmp_chan_send");
    }

    debug_printf("Waiting to get ack\n");
    bool can_receive = lmp_chan_can_recv(&rpc->channel);
    while(!can_receive){
      can_receive = lmp_chan_can_recv(&rpc->channel);
    }
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;
    err = lmp_chan_recv(&rpc -> channel, &msg, &NULL_CAP);
    if(err_is_fail(err) || msg.words[0] != AOS_RPC_ACK){
      debug_printf("First word should be ACK, is: %d\n",msg.words[0]);
      DEBUG_ERR(err,"Could not setup bi direction with init");
    }
    debug_printf("Acks has been received, channel is setup!\n");

    return err;
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
