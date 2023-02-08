#define _GNU_SOURCE      /* vasprintf */
#define _DARWIN_C_SOURCE /* vasprintf */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "report.h"
#include "xlog.h"

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
