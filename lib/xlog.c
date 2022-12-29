#define _POSIX_C_SOURCE 199509L /* flockfile, clock_gettime */

#include "toywasm_config.h"

#include <inttypes.h>
#if defined(TOYWASM_ENABLE_WASM_THREADS)
#include <pthread.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "nbio.h"
#include "xlog.h"

int xlog_tracing = 0;

/* NuttX: https://github.com/apache/incubator-nuttx/pull/6152 */
#if defined(__wasi__) || defined(__NuttX__)
#define flockfile(f)
#define funlockfile(f)
#endif

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
        nbio_fprintf(stderr, "%s (%ju.%09ld): ", buf, (uintmax_t)ts.tv_sec,
                     ts.tv_nsec);
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        pthread_t self = pthread_self();
        nbio_fprintf(stderr, "[%jx] ", (uintmax_t)self);
#endif
        nbio_vfprintf(stderr, fmt, ap);
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
        nbio_vfprintf(stderr, fmt, ap);
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
