#include <aos/aos.h>
#include <aos/dispatcher_rpc.h>
#include <aos/io_channels.h>

int stdout_write(const char *buf, size_t len);
int stdout_write(const char *buf, size_t len)
{
    /*debug_printf("w: %d, %s\n", len, buf);
    aos_dc_send(&stdout_chan, len, buf);
    return len;*/
    return aos_terminal_write(buf, len);
}

int stdin_read(char *buf, size_t len);
int stdin_read(char *buf, size_t len)
{
    return aos_terminal_read(buf, len);
    size_t recvd = 0;

    aos_dc_receive_available(&stdin_chan, len, buf, &recvd);
    while (recvd == 0) {
        debug_printf("hsa\n");
        event_dispatch(get_default_waitset());
        debug_printf("hsa2\n");
        aos_dc_receive_available(&stdin_chan, len, buf, &recvd);
    }
    return recvd;
}