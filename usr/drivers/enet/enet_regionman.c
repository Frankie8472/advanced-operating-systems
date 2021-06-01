#include <devif/queue_interface_backend.h>
#include <devif/backends/net/enet_devif.h>
#include <aos/aos.h>
#include <stdlib.h>
#include <assert.h>

#include "enet_regionman.h"

/**
 * \brief Initialize an enet-qstate-struct.
 */
errval_t init_enet_qstate(struct enet_queue* queue,
                                 struct enet_qstate* tgt) {
    assert(queue != NULL);
    tgt->queue = queue;
    tgt->free = NULL;
    return SYS_ERR_OK;
}

/**
 * \brief Add a new free buffer to an enet-qstate.
 * NOTE: susceptible to heavily reuse a small number of buffs
 * \param fr Node containing the free devq_buf to be added.
 */
void qstate_append_free(struct enet_qstate* qs,
                               struct dev_list* fr) {
    fr->next = qs->free;
    qs->free = fr;
}

/**
 * \brief Dequeues as many buffers as it can.
 * NOTE: the 'content' of the buffers is completely ignored.
 * DO NOT USE THIS FOR THE RECEIVE QUEUE!!!! only for the send queue
 */
errval_t dequeue_bufs(struct enet_qstate* qs) {
    struct devq_buf buf;
    errval_t err;

    err = devq_dequeue((struct devq*) qs->queue, &buf.rid, &buf.offset,
                       &buf.length, &buf.valid_data, &buf.valid_length,
                       &buf.flags);

    while (err_is_ok(err)) {
        struct devq_buf* nub = calloc(1, sizeof(struct devq_buf));
        struct dev_list* nul = calloc(1, sizeof(struct dev_list));
        *nub = buf;
        nul->cur = nub;
        qstate_append_free(qs, nul);

        err = devq_dequeue((struct devq*) qs->queue, &buf.rid, &buf.offset,
                           &buf.length, &buf.valid_data, &buf.valid_length,
                           &buf.flags);
    }

    return err;
}

/**
 * \brief Retrieve a free buffer from a qstate.
 * NOTE: atm, if the buffer is never put back into the queue, the enet
 * devq might run out of memory.
 * also, if no free buffer is available, "dequeue_bufs" will be called
 * make sure, this is never called on the in-queue, only the out-queue.
 */
errval_t get_free_buf(struct enet_qstate* qs, struct devq_buf* ret) {
    if (qs->free == NULL) {
        dequeue_bufs(qs);

        if (qs->free == NULL) {
            // NOTE: could also create and add new region here if wanted
            return DEVQ_ERR_NO_FREE_BUFFER;
        }
    }

    struct dev_list* fr = qs->free;
    struct devq_buf* fb = fr->cur;

    qs->free = qs->free->next;

    *ret = *fb;
    free(fr);
    free(fb);
    return SYS_ERR_OK;
}

/**
 * \brief Put a new buf into a queue.
 */
errval_t enqueue_buf(struct enet_qstate* qs, struct devq_buf* buf) {
    return devq_enqueue((struct devq*) qs->queue, buf->rid, buf->offset,
                        buf->length, buf->valid_data, buf->valid_length,
                        buf->flags);
}
