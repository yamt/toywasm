void xlog_printf(const char *, ...)
        __attribute__((__format__(__printf__, 1, 2)));

void xlog_printf_raw(const char *, ...)
        __attribute__((__format__(__printf__, 1, 2)));

void xlog__trace(const char *, ...)
        __attribute__((__format__(__printf__, 1, 2)));
void xlog_error(const char *, ...)
        __attribute__((__format__(__printf__, 1, 2)));

extern int xlog_tracing;

#define xlog_trace(...)                                                       \
        do {                                                                  \
                if (xlog_tracing) {                                           \
                        xlog__trace(__VA_ARGS__);                             \
                }                                                             \
        } while (false)\
