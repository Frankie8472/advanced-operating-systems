#ifndef BARRELFISH_UMP_CHAN_H
#define BARRELFISH_UMP_CHAN_H

#include <aos/types.h>
#include <errors/errno.h>
#include <assert.h>
#include <aos/waitset.h>
#include <aos/lmp_chan_arch.h>
#include <aos/lmp_chan.h>

#define UMP_MSG_SIZE 64 // TODO: set to cache line size
#define UMP_MSG_N_WORDS 7

#define UMP_FLAG_SENT 1 // flag for sent slots / to be received & ackd
#define UMP_FLAG_RECEIVED 0 // flag for ackd / open slots

struct ump_msg
{
    uint8_t flag;

    uint64_t data[];
};

/**
 * \brief Declare an ump message for the channel `chan` on the stack.
 * `chan` will then be initialized with type `struct ump_msg*`, pointing
 * to the according stack location.
 */
#define DECLARE_MESSAGE(chan, msg_name) uint64_t _temp_##msg_name[1 + ump_chan_get_data_len(&(chan))]; \
    struct ump_msg *msg_name = (struct ump_msg *) &_temp_##msg_name;

/* static_assert(sizeof(struct ump_msg) == UMP_MSG_SIZE, "ump_msg needs to be 64 bytes"); */

struct ump_chan
{
    size_t msg_size; /// < size of a single ump message

    void *recv_pane;
    size_t recv_pane_size;
    size_t recv_buf_index;

    void *send_pane;
    size_t send_pane_size;
    size_t send_buf_index;

    /// whether receiving messages on this channel is done by repeatedly polling
    /// or an ipi notification is expected when a message is receivable
    bool local_is_pinged;

    /// whether the other endpoint is operating in `local_is_pinged` mode,
    /// i.e. whether we need to send an ipi after each message we sent
    bool remote_is_pinged;

    union {
        /// when operating in polled mode, this chanstate is registered in the
        /// default waitset as a polled channel where it will regularly get polled
        struct waitset_chanstate waitset_state;

        /// when operating in pinged mode, this endpoint is registered to receive
        /// an empty lmp message when a ump message is available
        struct lmp_endpoint *lmp_ep;
    };

    /// if the remote end of the channel is operating in pinged mode (`remote_is_pinged`)
    /// we invoke this capability `invoke_ipi_notify` to notify the other end of a new message
    struct capref ipi_ep;
};

errval_t ump_chan_init_size(struct ump_chan *chan, size_t msg_size,
                            void *send_buf, size_t send_buf_size,
                            void *recv_buf, size_t recv_buf_size);

errval_t ump_chan_init_len(struct ump_chan *chan, int len,
                           void *send_buf, size_t send_buf_size,
                           void *recv_buf, size_t recv_buf_size);

errval_t ump_chan_init_default(struct ump_chan *chan,
                       void *send_buf, size_t send_buf_size,
                       void *recv_buf, size_t recv_buf_size);

errval_t ump_chan_destroy(struct ump_chan *chan);

int ump_chan_get_data_len(struct ump_chan *chan);

bool ump_chan_send(struct ump_chan *chan, struct ump_msg *send);

bool ump_chan_can_receive(struct ump_chan *chan);
bool ump_chan_poll_once(struct ump_chan *chan, struct ump_msg *recv);


/**
 * \brief switch to pinged mode on the local end
 * 
 * This function assumes a setup ump channel in local polled mode.
 * It deregisters the channel from the polled queue and will reregister
 * for messages received on `ep`.
 * 
 * \param ep needs to be a valid endpoint that is notified when we receive a
 *           message on this channel
 */
errval_t ump_chan_switch_local_pinged(struct ump_chan *chan, struct lmp_endpoint *ep);

errval_t ump_chan_switch_remote_pinged(struct ump_chan *chan, struct capref ipi_endpoint);


errval_t ump_chan_register_recv(struct ump_chan *chan, struct waitset *ws, struct event_closure closure);
errval_t ump_chan_deregister_recv(struct ump_chan *chan);

errval_t ump_chan_register_polled_recv(struct ump_chan *chan, struct waitset *ws, struct event_closure closure);

errval_t ump_chan_register_pinged_recv(struct ump_chan *chan, struct waitset *ws, struct event_closure closure);







// dedicated polling thread deprecated

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

errval_t ump_chan_register_polling(struct ump_poller *poller, struct ump_chan *chan, ump_msg_handler_t handler, void *arg);

errval_t ump_chan_run_poller(struct ump_poller *poller);

struct ump_poller *ump_chan_get_default_poller(void);

#endif // BARRELFISH_UMP_CHAN_H
