#include <aos/aos_datachan.h>

#define CLOSE_MESSAGE (~((uintptr_t) 0))


void aos_dc_buffer_init(struct aos_dc_ringbuffer *buf, size_t bytes, char *buffer)
{
    buf->length = bytes;
    buf->buffer = buffer;
    buf->read = 0;
    buf->write = 0;
}


size_t aos_dc_write_into_buffer(struct aos_dc_ringbuffer *buf, size_t bytes, const char *data)
{
    size_t written = 0;
    // free contiguous memory that we can write into either till ringbuffer end or read pointer
    size_t free_contiguous = min(buf->length - buf->write, buf->read - buf->write - 1);
    size_t to_write = min(free_contiguous, bytes);

    memcpy(buf->buffer + buf->write, data, to_write);
    buf->write += to_write;
    written += to_write;

    if (buf->write == buf->length && buf->read != 0) {
        buf->write = 0;
        bytes -= to_write;
        data += to_write;

        free_contiguous = buf->read - 1;
        to_write = min(free_contiguous, bytes);

        memcpy(buf->buffer + buf->write, data, to_write);
        buf->write += to_write;

        written += to_write;
        return written;
    }
    else {
        return written;
    }
}



size_t aos_dc_read_from_buffer(struct aos_dc_ringbuffer *buf, size_t bytes, char *data)
{
    size_t read = 0;
    if (buf->read > buf->write) {
        // read until the end of buffer
        size_t to_read = min(bytes, buf->length - buf->read);
        memcpy(data, buf->buffer + buf->read, to_read);
        buf->read += to_read;
        data += to_read;
        bytes -= to_read;
        read += to_read;
    }

    // wrap around
    if (buf->read == buf->length) {
        buf->read = 0;
    }

    if (buf->read < buf->write) {
        size_t to_read = min(bytes, buf->write - buf->read);
        memcpy(data, buf->buffer + buf->read, to_read);
        buf->read += to_read;
        data += to_read;
        bytes -= to_read;
        read += to_read;
    }

    return read;
}


size_t aos_dc_bytes_available(struct aos_dc_ringbuffer *buf)
{
    if (buf->read <= buf->write) {
        return buf->write - buf->read;
    }
    else {
        return (buf->length - buf->read) + buf->write;
    }
}


errval_t aos_dc_init_lmp(struct aos_datachan *dc, size_t buffer_length)
{
    char *buffer = malloc(buffer_length);
    NULLPTR_CHECK(buffer, LIB_ERR_MALLOC_FAIL);
    aos_dc_buffer_init(&dc->buffer, buffer_length, buffer);
    dc->backend = AOS_RPC_LMP;
    dc->is_closed = false;

    return SYS_ERR_OK;
}


errval_t aos_dc_init_ump(struct aos_datachan *dc, size_t buffer_length, lvaddr_t ump_page, size_t ump_page_size, bool first_half)
{
    char *buffer = malloc(buffer_length);
    NULLPTR_CHECK(buffer, LIB_ERR_MALLOC_FAIL);
    aos_dc_buffer_init(&dc->buffer, buffer_length, buffer);
    dc->backend = AOS_RPC_UMP;
    dc->is_closed = false;

    void *send_block;
    void *recv_block;
    if (first_half) {
        send_block = (void *) ump_page;
        recv_block = (void *) ump_page + ump_page_size / 2;
    }
    else {
        send_block = (void *) ump_page + ump_page_size / 2;
        recv_block = (void *) ump_page;
    }

    return ump_chan_init(&dc->channel.ump, send_block, ump_page_size / 2, recv_block, ump_page_size / 2);
}


errval_t aos_dc_free(struct aos_datachan *dc)
{
    free(dc->buffer.buffer);
    return SYS_ERR_OK;
}


bool aos_dc_send_is_connected(struct aos_datachan *dc)
{
    if (dc->backend == AOS_RPC_LMP) {
        return !capref_is_null(dc->channel.lmp.remote_cap);
    }
    else if (dc->backend == AOS_RPC_UMP) {
        return dc->channel.ump.send_pane != NULL;
    }
    return false;
}


static errval_t aos_dc_send_lmp(struct aos_datachan *dc, size_t bytes, const char *data)
{
    errval_t err;

    if (capref_is_null(dc->channel.lmp.remote_cap)) {
        return LIB_ERR_LMP_NOT_CONNECTED;
    }

    size_t lmp_bytes_length = LMP_MSG_LENGTH * sizeof(uintptr_t);
    size_t first_msg_length = (LMP_MSG_LENGTH - 1) * sizeof(uintptr_t);

    uintptr_t msg[LMP_MSG_LENGTH];

    msg[0] = bytes;
    memcpy(&msg[1], data, min(first_msg_length, bytes));
    do {
        err = lmp_chan_send(&dc->channel.lmp, LMP_FLAG_YIELD | (bytes > first_msg_length ? LMP_FLAG_SYNC : 0), NULL_CAP, 4, msg[0], msg[1], msg[2], msg[3]);
    } while (err_is_fail(err) && lmp_err_is_transient(err));

    for (size_t offs = first_msg_length; offs < bytes; offs += lmp_bytes_length) {
        memcpy(msg, data + offs, min(lmp_bytes_length, bytes - offs));

        bool is_last = offs + lmp_bytes_length >= bytes;

        do {
            if (is_last) {
                err = lmp_chan_send(&dc->channel.lmp, LMP_FLAG_YIELD | LMP_FLAG_SYNC, NULL_CAP, 4, msg[0], msg[1], msg[2], msg[3]);
            }
            else {
                err = lmp_chan_send(&dc->channel.lmp, LMP_FLAG_YIELD, NULL_CAP, 4, msg[0], msg[1], msg[2], msg[3]);
            }
        } while (err_is_fail(err) && lmp_err_is_transient(err));
    }

    return SYS_ERR_OK;
}


static errval_t aos_dc_send_ump(struct aos_datachan *dc, size_t bytes, const char *data)
{
    DECLARE_MESSAGE(dc->channel.ump, msg);


    size_t ump_bytes_length = ump_chan_get_data_len(&dc->channel.ump) * sizeof(uintptr_t);
    size_t first_msg_length = ump_bytes_length - sizeof(uintptr_t);

    msg->data[0] = bytes;
    memcpy(&msg->data[1], data, min(first_msg_length, bytes));

    while(!ump_chan_send(&dc->channel.ump, msg));

    for (size_t offs = first_msg_length; offs < bytes; offs += ump_bytes_length) {
        memcpy(&msg->data[0], data + offs, min(ump_bytes_length, bytes - offs));
        while(!ump_chan_send(&dc->channel.ump, msg));
    }

    return SYS_ERR_OK;
}


errval_t aos_dc_send(struct aos_datachan *dc, size_t bytes, const char *data)
{
    if (dc->backend == AOS_RPC_LMP) {
        return aos_dc_send_lmp(dc, bytes, data);
    }
    else if (dc->backend == AOS_RPC_UMP) {
        return aos_dc_send_ump(dc, bytes, data);
    }
    return SYS_ERR_NOT_IMPLEMENTED;
}


/**
 * \brief spins until at least one message is available, reads it and writes it into the buffer
 */
static errval_t aos_dc_receive_one_message_lmp(struct aos_datachan *dc)
{
    errval_t err;
    struct lmp_recv_msg msg = LMP_RECV_MSG_INIT;

    while (!lmp_chan_can_recv(&dc->channel.lmp)) {
        thread_yield();
    }

    do {
        err = lmp_chan_recv(&dc->channel.lmp, &msg, NULL);
        if (err_is_fail(err) && lmp_err_is_transient(err)) {
            thread_yield();
        }
    } while (err_is_fail(err) && lmp_err_is_transient(err));

    if (dc->bytes_left > 0) {
        // part of a multi message chunk
        size_t to_read = min(dc->bytes_left, LMP_MSG_LENGTH * sizeof(uintptr_t));
        size_t written = aos_dc_write_into_buffer(&dc->buffer, to_read, (const char *) &msg.words[0]);
        dc->bytes_left -= to_read;
        if (written < to_read) {
            return SYS_ERR_LMP_BUF_OVERFLOW; // TODO: err code
        }
    }
    else {
        //debug_printf("msg.words[0] = 0x%lx\n", msg.words[0]);
        //debug_printf("msg.words[1] = 0x%lx\n", msg.words[1]);
        if (msg.words[0] == CLOSE_MESSAGE) {
            dc->is_closed = true;
            return true;
        }

        dc->bytes_left = msg.words[0];
        size_t to_read = min(dc->bytes_left, (LMP_MSG_LENGTH - 1) * sizeof(uintptr_t));
        size_t written = aos_dc_write_into_buffer(&dc->buffer, to_read, (const char *) &msg.words[1]);
        dc->bytes_left -= to_read;
        if (written < to_read) {
            return SYS_ERR_LMP_BUF_OVERFLOW; // TODO: err code
        }
    }

    return SYS_ERR_OK;
}


/**
 * \brief spins until at least one message is available, reads it and writes it into the buffer
 */
static errval_t aos_dc_receive_one_message_ump(struct aos_datachan *dc)
{
    while (!ump_chan_can_receive(&dc->channel.ump)) {
        thread_yield();
    }

    DECLARE_MESSAGE(dc->channel.ump, msg);

    while(!ump_chan_poll_once(&dc->channel.ump, msg));

    if (dc->bytes_left > 0) {
        // part of a multi message chunk
        size_t to_read = min(dc->bytes_left, ump_chan_get_data_len(&dc->channel.ump) * sizeof(uintptr_t));
        size_t written = aos_dc_write_into_buffer(&dc->buffer, to_read, (const char *) &msg->data[0]);
        dc->bytes_left -= to_read;
        if (written < to_read) {
            return SYS_ERR_LMP_BUF_OVERFLOW; // TODO: err code
        }
    }
    else {
        if (msg->data[0] == CLOSE_MESSAGE) {
            dc->is_closed = true;
            return true;
        }

        dc->bytes_left = msg->data[0];
        size_t to_read = min(dc->bytes_left, (ump_chan_get_data_len(&dc->channel.ump) - 1) * sizeof(uintptr_t));
        size_t written = aos_dc_write_into_buffer(&dc->buffer, to_read, (const char *) &msg->data[1]);
        dc->bytes_left -= to_read;
        if (written < to_read) {
            return SYS_ERR_LMP_BUF_OVERFLOW; // TODO: err code
        }
    }

    return SYS_ERR_OK;
}


static errval_t aos_dc_receive_buffered(struct aos_datachan *dc, size_t bytes, char *data)
{
    while(bytes > 0) {
        if (aos_dc_bytes_available(&dc->buffer) != 0) {
            size_t read = aos_dc_read_from_buffer(&dc->buffer, bytes, data);
            bytes -= read;
            data += read;
        }
        else {
            if (dc->backend == AOS_RPC_LMP) {
                errval_t err = aos_dc_receive_one_message_lmp(dc);
                ON_ERR_RETURN(err);
            }
            else if (dc->backend == AOS_RPC_UMP) {
                errval_t err = aos_dc_receive_one_message_ump(dc);
                ON_ERR_RETURN(err);
            }
            else {
                return LIB_ERR_NOT_IMPLEMENTED;
            }
        }
    }
    return SYS_ERR_OK;
}


errval_t aos_dc_receive_available(struct aos_datachan *dc, size_t bytes, char *data, size_t *received)
{
    errval_t err = SYS_ERR_OK;
    size_t read = aos_dc_read_from_buffer(&dc->buffer, bytes, data);

    while (read < bytes && (
                (dc->backend == AOS_RPC_LMP &&
                    lmp_chan_can_recv(&dc->channel.lmp))
                    ||
                (dc->backend == AOS_RPC_UMP &&
                    ump_chan_can_receive(&dc->channel.ump))
    )) {
        if (dc->is_closed) {
            break;
        }

        if (dc->backend == AOS_RPC_LMP) {
            err = aos_dc_receive_one_message_lmp(dc);
        }
        else if (dc->backend == AOS_RPC_UMP) {
            err = aos_dc_receive_one_message_ump(dc);
        }
        ON_ERR_RETURN(err);
        read += aos_dc_read_from_buffer(&dc->buffer, bytes - read, data + read);
    }

     *received = read;
    return SYS_ERR_OK;
}


errval_t aos_dc_can_receive(struct aos_datachan *dc)
{
    if (aos_dc_bytes_available(&dc->buffer) > 0) {
        return true;
    }
    else if (dc->backend == AOS_RPC_LMP) {
        return lmp_chan_can_recv(&dc->channel.lmp);
    }
    else if (dc->backend == AOS_RPC_UMP) {
        return ump_chan_can_receive(&dc->channel.ump);
    }
    return false;
}


errval_t aos_dc_receive(struct aos_datachan *dc, size_t bytes, char *data, size_t *received)
{
    *received = 0;
    do {
        errval_t err = aos_dc_receive_available(dc, bytes, data, received);
        ON_ERR_RETURN(err);
        if (*received == 0) {
            thread_yield();
        }
        if (dc->is_closed) {
            break;
        }
    } while(*received == 0);
    return SYS_ERR_OK;
}


errval_t aos_dc_receive_all(struct aos_datachan *dc, size_t bytes, char *data)
{
    errval_t err;

    while (true) {
        size_t buffered = 0;

        err = aos_dc_receive_available(dc, bytes, data, &buffered);

        if (buffered == bytes) {
            // there was enough data in the buffer, no need to wait for more
            return SYS_ERR_OK;
        }

        data += buffered;
        bytes -= buffered;

        // read the rest
        return aos_dc_receive_buffered(dc, bytes, data);
    }

    return SYS_ERR_OK;
}

errval_t aos_dc_register(struct aos_datachan *dc, struct waitset *ws, struct event_closure closure)
{
    if (dc->backend == AOS_RPC_LMP) {
        return lmp_endpoint_register(dc->channel.lmp.endpoint, ws, closure);
    }
    else if (dc->backend == AOS_RPC_UMP) {
        return ump_chan_register_recv(&dc->channel.ump, ws, closure);
    }
    return LIB_ERR_NOT_IMPLEMENTED;
}

errval_t aos_dc_deregister(struct aos_datachan *dc)
{
    if (dc->backend == AOS_RPC_LMP) {
        return lmp_endpoint_deregister(dc->channel.lmp.endpoint);
    }
    else if (dc->backend == AOS_RPC_UMP) {
        return ump_chan_deregister_recv(&dc->channel.ump);
    }
    return LIB_ERR_NOT_IMPLEMENTED;
}


bool aos_dc_is_closed(struct aos_datachan *dc)
{
    return dc->is_closed;
}

/**
 * \brief sends a close message through the channel
 * 
 * \param dc 
 * \return errval_t 
 */
errval_t aos_dc_close(struct aos_datachan *dc)
{
    errval_t err;
    dc->is_closed = true;
    if (dc->backend == AOS_RPC_LMP) {
        do {
            err = lmp_chan_send4(&dc->channel.lmp, LMP_FLAG_SYNC | LMP_FLAG_YIELD, NULL_CAP, CLOSE_MESSAGE, 0, 0, 0);
        } while (err_is_fail(err) && lmp_err_is_transient(err));
        if (err_is_fail(err)) {
            return err;
        }
    }
    else if (dc->backend == AOS_RPC_UMP) {
        DECLARE_MESSAGE(dc->channel.ump, msg);

        msg->data[0] = CLOSE_MESSAGE;

        while(!ump_chan_send(&dc->channel.ump, msg));
    }
    return SYS_ERR_OK;
}
