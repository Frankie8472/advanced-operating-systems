#include <aos/ump_chan.h>

#include <aos/aos.h>

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

bool ump_chan_send(struct ump_chan *chan, struct ump_msg *send)
{
    void *send_location = chan->send_pane + chan->send_buf_index * UMP_MSG_SIZE;
    
    // ensure cache line alignedness
    assert(((lvaddr_t) send_location) % UMP_MSG_SIZE == 0);

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

