#include "platform.h"
#include "toywasm_config.h"

__BEGIN_EXTERN_C

void xlog_printf(const char *, ...)
        __attribute__((__format__(__printf__, 1, 2)));

void xlog_printf_raw(const char *, ...)
        __attribute__((__format__(__printf__, 1, 2)));

void xlog__trace(const char *, ...)
        __attribute__((__format__(__printf__, 1, 2)));
void xlog_error(const char *, ...)
        __attribute__((__format__(__printf__, 1, 2)));

#if defined(TOYWASM_ENABLE_TRACING)
extern int xlog_tracing;
#endif

__END_EXTERN_C

#if defined(TOYWASM_ENABLE_TRACING_INSN)
#define xlog_trace_insn(...)                                                  \
        do {                                                                  \
                if (__predict_false(xlog_tracing > 1)) {                      \
                        xlog__trace(__VA_ARGS__);                             \
                }                                                             \
        } while (0)
#else
#define xlog_trace_insn(...)
#endif

#if defined(TOYWASM_ENABLE_TRACING)
#define xlog_trace(...)                                                       \
        do {                                                                  \
                if (__predict_false(xlog_tracing > 0)) {                      \
                        xlog__trace(__VA_ARGS__);                             \
                }                                                             \
        } while (0)
#else
#define xlog_trace(...)
#endif
