#if !defined(_TOYWASM_REPORT_H)
#define _TOYWASM_REPORT_H

#include <stdarg.h>

#include "platform.h"

struct report {
        char *msg;
};

__BEGIN_EXTERN_C

void vreport(struct report *r, const char *fmt, va_list ap);
void report_error(struct report *r, const char *fmt, ...)
        __attribute__((__format__(__printf__, 2, 3)));
void report_init(struct report *r);
void report_clear(struct report *r);
const char *report_getmessage(const struct report *r);

__END_EXTERN_C

#endif /* defined(_TOYWASM_REPORT_H) */
