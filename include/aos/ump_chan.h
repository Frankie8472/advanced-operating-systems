#ifndef BARRELFISH_UMP_CHAN_H
#define BARRELFISH_UMP_CHAN_H

#include <aos/types.h>
#include <errors/errno.h>
#include <assert.h>

#define UMP_MSG_SIZE 64 // TODO: set to cache line size

#define UMP_FLAG_SENT 1 // flag for sent slots / to be received & ackd
#define UMP_FLAG_RECEIVED 0 // flag for ackd / open slots

struct ump_msg
{
    union {
        uint64_t u64[7];
    } data;

    uint8_t flag;
};

static_assert(sizeof(struct ump_msg) == UMP_MSG_SIZE, "ump_msg needs to be 64 bytes");

struct ump_chan
{
    void *recv_pane;
    size_t recv_pane_size;
    size_t recv_buf_index;

    void *send_pane;
    size_t send_pane_size;
    size_t send_buf_index;
};


errval_t ump_chan_init(struct ump_chan *chan,
                       void *send_buf, size_t send_buf_size,
                       void *recv_buf, size_t recv_buf_size);

bool ump_chan_send(struct ump_chan *chan, struct ump_msg *send);

bool ump_chan_poll_once(struct ump_chan *chan, struct ump_msg *recv);


typedef errval_t (*ump_msg_handler_t)(void *arg, struct ump_msg *msg);

struct ump_poller
{
    struct ump_chan **channels;
    ump_msg_handler_t *handlers;
    void **args;
    size_t n_channels;
    size_t capacity_channels;
};

errval_t ump_chan_init_poller(struct ump_poller *poller);

errval_t ump_chan_register_polling(struct ump_poller *poller, struct ump_chan *chan, ump_msg_handler_t handler);

errval_t ump_chan_run_poller(struct ump_poller *poller);

#endif // BARRELFISH_UMP_CHAN_H
