#ifndef AOS_IO_CHANNELS_H
#define AOS_IO_CHANNELS_H

#include <aos/aos.h>

size_t aos_terminal_write(const char *buf, size_t len);

size_t aos_terminal_read(char *buf, size_t len);

#endif // AOS_IO_CHANNELS_H
