#include <aos/ump_chan.h>

#include <aos/aos.h>
#include <aos/dispatcher_arch.h>
#include <aos/waitset.h>
#include "waitset_chan_priv.h"

/**
 * \brief Initialize an ump_chan struct. Please make sure that both the send- and
 * the receive-buffer have all their flag-bytes (or just everything to be sure) set
 * to 0 before initializing the channel from either side.
 * \param chan Pointer to instance to initialize
 * \param send_buf, send_buf_size Location and size of the channel's send-buffer
 * \param recv_buf, recv_buf_size Location and size of the channel's receive-buffer
 */
errval_t ump_chan_init(struct ump_chan *chan,
                       void *send_buf, size_t send_buf_size,
                       void *recv_buf, size_t recv_buf_size)
{
    chan->send_pane = send_buf;
    chan->send_pane_size = send_buf_size;
    chan->send_buf_index = 0;

    chan->recv_pane = recv_buf;
    chan->recv_pane_size = recv_buf_size;
    chan->recv_buf_index = 0;
    
    chan->waitset_state.chantype = CHANTYPE_UMP_IN;
    chan->waitset_state.state = CHAN_UNREGISTERED; 
    chan->waitset_state.arg = chan;

    return SYS_ERR_OK;
}

/**
 * \brief Send a message over an ump-channel.
 * \param chan Channel to use.
 * \param send Pointer to the message to send.
 * \return Flag: true if the message could be sent, false if the
 * message could not be sent (because send-buffer is full).
 */
bool ump_chan_send(struct ump_chan *chan, struct ump_msg *send)
{
    void *send_location = chan->send_pane + chan->send_buf_index * UMP_MSG_SIZE;
    
    // ensure cache line alignedness
    assert(((lvaddr_t) send_location) % UMP_MSG_SIZE == 0);
    
    // assert flag state
    send->flag = 0;

    struct ump_msg *write = send_location;
    if (write->flag != UMP_FLAG_RECEIVED) // check if previous msg at location was acked
        return false;

    dmb();  // write after check
    memcpy(write, send, UMP_MSG_SIZE);

    dmb();  // set after write
    write->flag = UMP_FLAG_SENT;

    chan->send_buf_index++;
    chan->send_buf_index %= chan->send_pane_size / UMP_MSG_SIZE;
    return true;
}


bool ump_chan_can_receive(struct ump_chan *chan)
{
    void *poll_location = chan->recv_pane + chan->recv_buf_index * UMP_MSG_SIZE;
    
    // ensure cache line alignedness
    assert(((lvaddr_t) poll_location) % UMP_MSG_SIZE == 0);
    
    struct ump_msg *read = poll_location;

    return read->flag == UMP_FLAG_SENT;
}

/**
 * \brief Poll an ump-channel for a new message.
 * \param chan Channel to poll
 * \param recv Location to write the result to.
 * \return Flag: true if a message was received and written to ´recv´,
 * false if no new message was received (and ´recv´ was not written to).
 */
bool ump_chan_poll_once(struct ump_chan *chan, struct ump_msg *recv)
{
    void *poll_location = chan->recv_pane + chan->recv_buf_index * UMP_MSG_SIZE;
    
    // ensure cache line alignedness
    assert(((lvaddr_t) poll_location) % UMP_MSG_SIZE == 0);
    
    struct ump_msg *read = poll_location;
    if (read->flag == UMP_FLAG_SENT) {

        dmb();
        memcpy(recv, read, UMP_MSG_SIZE);
        dmb();
        read->flag = UMP_FLAG_RECEIVED;

        chan->recv_buf_index++;
        chan->recv_buf_index %= chan->recv_pane_size / UMP_MSG_SIZE;
        
        return true;
    }
    
    return false;
}


errval_t ump_chan_register_recv(struct ump_chan *chan, struct waitset *ws, struct event_closure closure)
{
    errval_t err;

    dispatcher_handle_t handle = disp_disable();
    struct dispatcher_generic *dp = get_dispatcher_generic(handle);

    /*if (ump_chan_can_receive(chan)) { // trigger immediately
        err = waitset_chan_trigger_closure_disabled(ws, &chan->waitset_state,
                                                    closure, handle);
    }*/
    
    {
        err = waitset_chan_register_disabled(ws, &chan->waitset_state, closure);
        if (err_is_ok(err)) {
            /* enqueue on poll list */
            struct waitset_chanstate *cs = &chan->waitset_state;
            if (dp->polled_channels == NULL) {
                cs->polled_prev = cs;
                cs->polled_next = cs;
            } else {
                cs->polled_next = dp->polled_channels;
                cs->polled_prev = cs->polled_next->polled_prev;
                cs->polled_next->polled_prev = cs;
                cs->polled_prev->polled_next = cs;
            }
            dp->polled_channels = cs;
        }
    }

    disp_enable(handle);

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
            bool received = ump_chan_poll_once(poller->channels[i], &um);
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