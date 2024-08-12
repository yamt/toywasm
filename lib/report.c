#define _GNU_SOURCE      /* vasprintf */
#define _DARWIN_C_SOURCE /* vasprintf */
#define _NETBSD_SOURCE   /* vasprintf */

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "report.h"
#include "xlog.h"

#if defined(_WIN32)
int
vasprintf(char **resultp, const char *fmt, va_list ap)
{
        int ret;

        ret = vsnprintf(NULL, 0, fmt, ap);
        if (ret < 0) {
                return ret;
        }
        size_t bufsz = ret + 1; /* +1 for the terminating NUL */
        char *p = malloc(bufsz);
        if (p == NULL) {
                return -1;
        }
        ret = vsnprintf(p, bufsz, fmt, ap);
        if (ret < 0) {
                free(p);
                return ret;
        }
        assert(ret + 1 == bufsz);
        *resultp = p;
        return ret;
}
#endif

void
vreport(struct report *r, const char *fmt, va_list ap)
{
        if (r->msg != NULL) {
                return;
        }
        int ret;
        r->msg = NULL;
        ret = vasprintf(&r->msg, fmt, ap);
        if (ret < 0) {
                xlog_error("failed to format message with %d", errno);
        }
}

void
report_error(struct report *r, const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        vreport(r, fmt, ap);
        va_end(ap);
}

void
report_init(struct report *r)
{
        r->msg = NULL;
}

void
report_clear(struct report *r)
{
        free(r->msg);
        r->msg = NULL;
}

const char *
report_getmessage(const struct report *r)
{
        if (r->msg == NULL) {
                return "no message";
        }
        return r->msg;
}
