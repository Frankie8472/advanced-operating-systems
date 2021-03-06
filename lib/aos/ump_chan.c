#include <aos/ump_chan.h>

#include <aos/aos.h>
#include <aos/dispatcher_arch.h>
#include <aos/waitset.h>
#include <aos/waitset_chan.h>

/**
 * \brief Like ump_chan_init_default, just with (maybe) a different msg_size than UMP_MSG_SIZE
 * NOTE: make sure both involved processes initialize the channel with the same size!
 * otherwise, things gonna get weird af (meaning: not usable)
 */
errval_t ump_chan_init_size(struct ump_chan *chan, size_t msg_size,
                            void *send_buf, size_t send_buf_size,
                            void *recv_buf, size_t recv_buf_size) {
    chan->msg_size = msg_size;

    chan->send_pane = send_buf;
    chan->send_pane_size = send_buf_size;
    chan->send_buf_index = 0;

    chan->recv_pane = recv_buf;
    chan->recv_pane_size = recv_buf_size;
    chan->recv_buf_index = 0;

    waitset_chanstate_init(&chan->waitset_state, CHANTYPE_UMP_IN);
    chan->waitset_state.arg = chan;

    return SYS_ERR_OK;
}

/**
 * \brief Like ump_chan_init_default, just with (maybe) a different number of uint64_t per message
 * than 7
 */
errval_t ump_chan_init_len(struct ump_chan *chan, int len,
                           void *send_buf, size_t send_buf_size,
                           void *recv_buf, size_t recv_buf_size) {
    size_t msg_size = (1 + len) * sizeof(uintptr_t);
    assert(msg_size > 8);
    return ump_chan_init_size(chan, msg_size,
                              send_buf, send_buf_size,
                              recv_buf, recv_buf_size);
}

/**
 * \brief Initialize an ump_chan struct. Please make sure that both the send- and
 * the receive-buffer have all their flag-bytes (or just everything to be sure) set
 * to 0 before initializing the channel from either side.
 * \param chan Pointer to instance to initialize
 * \param send_buf, send_buf_size Location and size of the channel's send-buffer
 * \param recv_buf, recv_buf_size Location and size of the channel's receive-buffer
 */
errval_t ump_chan_init_default(struct ump_chan *chan,
                       void *send_buf, size_t send_buf_size,
                       void *recv_buf, size_t recv_buf_size)
{
    return ump_chan_init_size(chan, UMP_MSG_SIZE,
                              send_buf, send_buf_size,
                              recv_buf, recv_buf_size);
}


errval_t ump_chan_destroy(struct ump_chan *chan)
{
    waitset_chanstate_destroy(&chan->waitset_state);
    return SYS_ERR_OK;
}

/**
 * \brief Given a channel, return the number of uint64 elements found in a single
 * message's data array.
 * \param chan ump_chan instance pointer
 * \return Length of data array from a message inside that channel.
 */
int ump_chan_get_data_len(struct ump_chan *chan) {
    return (chan->msg_size / sizeof(uintptr_t)) - 1;
}

/**
 * \brief Send a message over an ump-channel.
 * \param chan Channel to use.
 * \param send Pointer to the message to send.
 * \param ping_if_pinged If the channel is operating in pinged mode, determines whether
 *                       a ping will be sent. Otherwise ignored.
 * \return true if the message could be sent, false if the
 * message could not be sent (because send-buffer is full).
 */
bool ump_chan_send(struct ump_chan *chan, struct ump_msg *send, bool ping_if_pinged)
{
    void *send_location = chan->send_pane + chan->send_buf_index * chan->msg_size;
    
    // ensure cache line alignedness
    assert(((lvaddr_t) send_location) % chan->msg_size == 0);
    
    // assert flag state
    send->flag = 0;

    struct ump_msg *write = send_location;
    if (write->flag != UMP_FLAG_RECEIVED) // check if previous msg at location was acked
        return false;

    dmb();  // write after check
    memcpy(write, send, chan->msg_size);

    dmb();  // set after write
    write->flag = UMP_FLAG_SENT;

    chan->send_buf_index++;
    chan->send_buf_index %= chan->send_pane_size / chan->msg_size;

    if (chan->remote_is_pinged && ping_if_pinged) {
        invoke_ipi_notify(chan->ipi_ep);
    }
    return true;
}


bool ump_chan_can_receive(struct ump_chan *chan)
{
    void *poll_location = chan->recv_pane + chan->recv_buf_index * chan->msg_size;
    
    // ensure cache line alignedness
    assert(((lvaddr_t) poll_location) % chan->msg_size == 0);
    
    struct ump_msg *read = poll_location;

    return read->flag == UMP_FLAG_SENT;
}

/**
 * \brief Poll an ump-channel for a new message.
 * \param chan Channel to poll
 * \param recv Location to write the result to.
 * \return Flag: true if a message was received and written to ??recv??,
 * false if no new message was received (and ??recv?? was not written to).
 */
bool ump_chan_receive(struct ump_chan *chan, struct ump_msg *recv)
{
    void *poll_location = chan->recv_pane + chan->recv_buf_index * chan->msg_size;
    
    // ensure cache line alignedness
    assert(((lvaddr_t) poll_location) % chan->msg_size == 0);
    
    struct ump_msg *read = poll_location;
    if (*((volatile uint8_t *) &read->flag) == UMP_FLAG_SENT) {

        dmb();
        memcpy(recv, read, chan->msg_size);
        dmb();
        read->flag = UMP_FLAG_RECEIVED;

        chan->recv_buf_index++;
        chan->recv_buf_index %= chan->recv_pane_size / chan->msg_size;
        
        return true;
    }
    
    return false;
}


errval_t ump_chan_switch_local_pinged(struct ump_chan *chan, struct lmp_endpoint *ep)
{
    assert(!chan->local_is_pinged);
    errval_t err;

    struct waitset *ws = chan->waitset_state.waitset;
    struct event_closure closure = chan->waitset_state.closure;

    if (ws == NULL) {
        return LIB_ERR_UMP_SWITCH_NO_WAITSET;
    }

    waitset_chan_deregister(&chan->waitset_state);

    err = lmp_endpoint_register(ep, ws, closure);
    ON_ERR_PUSH_RETURN(err, LIB_ERR_UMP_REGISTER_PINGED_EP);

    chan->local_is_pinged = true;
    // chan->waitset_state will now be overwritten
    chan->lmp_ep = ep;

    return SYS_ERR_OK;
}


errval_t ump_chan_switch_remote_pinged(struct ump_chan *chan, struct capref ipi_endpoint)
{
    chan->remote_is_pinged = true;
    chan->ipi_ep = ipi_endpoint;

    return SYS_ERR_OK;
}

errval_t ump_chan_register_recv(struct ump_chan *chan, struct waitset *ws, struct event_closure closure)
{
    if (chan->local_is_pinged) {
        return ump_chan_register_pinged_recv(chan, ws, closure);
    }
    else {
        return ump_chan_register_polled_recv(chan, ws, closure);
    }
}


errval_t ump_chan_deregister_recv(struct ump_chan *chan)
{

    if (chan->local_is_pinged) {
        return lmp_endpoint_deregister(chan->lmp_ep);
    }
    else {
        return waitset_chan_deregister(&chan->waitset_state);
    }
}


errval_t ump_chan_register_polled_recv(struct ump_chan *chan, struct waitset *ws, struct event_closure closure)
{
    return waitset_chan_register_polled(ws, &chan->waitset_state, closure);
}



errval_t ump_chan_register_pinged_recv(struct ump_chan *chan, struct waitset *ws, struct event_closure closure)
{
    errval_t err;

    err = lmp_endpoint_register(chan->lmp_ep, ws, closure);

    return err;
}





// dedicated polling thread deprecated

errval_t ump_chan_init_poller(struct ump_poller *poller)
{
    const size_t start_capacity = 8;
    poller->channels = malloc(sizeof (struct ump_channel *) * start_capacity);
    poller->handlers = malloc(sizeof (ump_msg_handler_t) * start_capacity);
    poller->args = malloc(sizeof (void *) * start_capacity);
    poller->n_channels = 0;
    poller->capacity_channels = start_capacity;
    
    return SYS_ERR_OK;
}

errval_t ump_chan_register_polling(struct ump_poller *poller, struct ump_chan *chan, ump_msg_handler_t handler, void *arg)
{
    // check for capacity limit reached
    if (poller->n_channels >= poller->capacity_channels) {
        size_t new_size = poller->capacity_channels * 2;
        struct ump_chan **new_channels = malloc(sizeof (struct ump_channel *) * new_size);
        ump_msg_handler_t *new_handlers = malloc(sizeof (ump_msg_handler_t) * new_size);
        void **new_args = malloc(sizeof (void *) * new_size);
        memcpy(new_channels, poller->channels, sizeof (struct ump_channel *) * poller->n_channels);
        memcpy(new_handlers, poller->handlers, sizeof (ump_msg_handler_t) * poller->n_channels);
        memcpy(new_args, poller->args, sizeof (void *) * poller->n_channels);
        free(poller->channels);
        free(poller->handlers);
        free(poller->args);
        poller->channels = new_channels;
        poller->handlers = new_handlers;
        poller->args = new_args;
    }

    poller->channels[poller->n_channels] = chan;
    poller->handlers[poller->n_channels] = handler;
    poller->args[poller->n_channels] = arg;
    poller->n_channels ++;
    return SYS_ERR_OK;
}




errval_t ump_chan_run_poller(struct ump_poller *poller)
{
    while(true) {
        volatile size_t *n_channels = &poller->n_channels;
        for (int i = 0; i < *n_channels; i++) {
            struct ump_msg um = { .flag = 0 };
            bool received = ump_chan_receive(poller->channels[i], &um);
            if (received) {
                poller->handlers[i](poller->args[i], &um);
            }
            else {
            }
        }
    }
}


struct ump_poller *ump_chan_get_default_poller(void)
{
    static struct ump_poller default_poller;
    static bool initialized = false;
    
    if (!initialized) {
        ump_chan_init_poller(&default_poller);
        initialized = true;
    }
    return &default_poller;
}
