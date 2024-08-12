#include <sys/types.h> /* ssize_t */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "platform.h"

__BEGIN_EXTERN_C

/*
 * The following nbio_xxx functions are intended to be convenient
 * replacements of the corresponding libc functions, where
 * underlying files can have the O_NONBLOCK flag set.
 *
 * Background:
 * - Our WASI implementation relies on all files being non-blocking
 *   to implement thread cancellation for proc_exit.
 * - As we usually shares host fds 0-2 with wasm instances, the rest of
 *   the code (namely xlog and repl) needs to deal with non-blocking fds.
 */
int nbio_vfprintf(FILE *fp, const char *fmt, va_list ap);
int nbio_fprintf(FILE *fp, const char *fmt, ...) __printflike(2, 3);
int nbio_printf(const char *fmt, ...) __printflike(1, 2);
#if !defined(_WIN32)
ssize_t nbio_getline(char **linep, size_t *linecapp, FILE *fp);
int set_nonblocking(int fd, bool nonblocking, bool *orig);
bool is_again(int error);
#endif

__END_EXTERN_C
