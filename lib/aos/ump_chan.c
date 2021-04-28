#include <aos/ump_chan.h>

#include <aos/aos.h>

/**
 * \brief Initialize an ump_chan struct.
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
    if (write->flag)  // check if the previous msg at location has been acked
        return false;

    dmb();  // write after check
    memcpy(write, send, UMP_MSG_SIZE);

    dmb();  // set after write
    write->flag = 'm';

    chan->send_buf_index++;
    chan->send_buf_index %= chan->send_pane_size / UMP_MSG_SIZE;
    return true;
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
    if (read->flag != 0) {

        dmb();
        memcpy(recv, read, UMP_MSG_SIZE);
        dmb();
        read->flag = 0;

        chan->recv_buf_index++;
        chan->recv_buf_index %= chan->recv_pane_size / UMP_MSG_SIZE;
        
        return true;
    }
    
    return false;
}


errval_t ump_chan_init_poller(struct ump_poller *poller)
{
    const size_t start_capacity = 8;
    poller->channels = malloc(sizeof (struct ump_channel *) * start_capacity);
    poller->handlers = malloc(sizeof (ump_msg_handler_t) * start_capacity);
    poller->n_channels = 0;
    poller->capacity_channels = start_capacity;
}

errval_t ump_chan_register_polling(struct ump_chan *chan, ump_msg_handler_t handler)
{
}


errval_t ump_chan_run_poller(struct ump_poller *poller)
{
    
}
