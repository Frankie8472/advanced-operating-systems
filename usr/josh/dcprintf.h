#ifndef JOSH_DCPRINTF_H
#define JOSH_DCPRINTF_H

#include <aos/aos.h>
#include <aos/aos_datachan.h>
#include <stdarg.h>

/**
 * \brief simple fprintf clone to write to a `struct aos_datachan`
 * 
 * \note this function can only write 2KiB at a time
 */
inline static int dcprintf(struct aos_datachan *chan, const char *fmt, ...) {
    char buf[2048];
	memset(buf, 0, sizeof buf);
	va_list vl;
	va_start(vl, fmt);
    size_t written = vsnprintf(buf, sizeof buf, fmt, vl);
	va_end(vl);
	aos_dc_send(chan, written, buf);
	return written;
}

#endif // JOSH_DCPRINTF_H

