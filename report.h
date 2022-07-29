#if !defined(_REPORT_H)
#define _REPORT_H

#include <stdarg.h>

struct report {
        char *msg;
};

void vreport(struct report *r, const char *fmt, va_list ap);
void report_error(struct report *r, const char *fmt, ...);
void report_init(struct report *r);
void report_clear(struct report *r);
#endif /* defined(_REPORT_H) */
