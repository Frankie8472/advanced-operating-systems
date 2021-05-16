#include <aos/aos.h>
#include <aos/dispatcher_rpc.h>

int stdout_write(const char *buf, size_t len);
int stdout_write(const char *buf, size_t len)
{
    aos_dc_send(&stdout_chan, len, buf);
    return len;
}

int stdin_read(char *buf, size_t len);
int stdin_read(char *buf, size_t len)
{
    size_t recvd;
    aos_dc_receive(&stdin_chan, len, buf, &recvd);
    return recvd;
}