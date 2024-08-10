#if !defined(_TOYWASM_REPORT_H)
#define _TOYWASM_REPORT_H

#include <stdarg.h>

#include "platform.h"

struct report {
        char *msg;
};

__BEGIN_EXTERN_C

/*
 * APIs to report descriptive messages for human users
 *
 * Note: if report_error is called multiple times, the first one wins.
 */

void vreport(struct report *r, const char *fmt, va_list ap);
void report_error(struct report *r, const char *fmt, ...) __printflike(2, 3);
void report_init(struct report *r);
void report_clear(struct report *r);
const char *report_getmessage(const struct report *r);

__END_EXTERN_C

#endif /* defined(_TOYWASM_REPORT_H) */
