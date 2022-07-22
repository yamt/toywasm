#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "xlog.h"

int xlog_tracing = 0;

void
xlog_vprintf(const char *fmt, va_list ap)
{
        struct timespec ts;

        clock_gettime(CLOCK_REALTIME, &ts);

        time_t clock = ts.tv_sec;
        struct tm tm;
        char buf[sizeof("0000-00-00 00-00-00")];
        strftime(buf, sizeof(buf), "%F %T", localtime_r(&clock, &tm));

        flockfile(stderr);
        fprintf(stderr, "%s (%ju.%09ld): ", buf, (uintmax_t)ts.tv_sec,
                ts.tv_nsec);
        vfprintf(stderr, fmt, ap);
        funlockfile(stderr);
}

void
xlog_printf(const char *fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
        xlog_vprintf(fmt, ap);
        va_end(ap);
}
void
xlog_printf_raw(const char *fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
}

void
xlog__trace(const char *fmt, ...)
{
        if (!xlog_tracing) {
                return;
        }
        va_list ap;
        va_start(ap, fmt);
        flockfile(stderr);
        xlog_vprintf(fmt, ap);
        xlog_printf_raw("\n");
        funlockfile(stderr);
        va_end(ap);
}

void
xlog_error(const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        flockfile(stderr);
        xlog_vprintf(fmt, ap);
        xlog_printf_raw("\n");
        funlockfile(stderr);
        va_end(ap);
}
