#define _GNU_SOURCE      /* vasprintf, getline */
#define _DARWIN_C_SOURCE /* vasprintf, getline */
#define _NETBSD_SOURCE   /* vasprintf, getline */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nbio.h"

/* NuttX: https://github.com/apache/incubator-nuttx/pull/6152 */
#if defined(__wasi__) || defined(__NuttX__)
#define flockfile(f)
#define funlockfile(f)
#endif

int
set_nonblocking(int fd, bool nonblocking, bool *orig)
{
        int flags = fcntl(fd, F_GETFL, 0);
        int newflags = flags & ~O_NONBLOCK;
        if (nonblocking) {
                newflags |= O_NONBLOCK;
        }
        int ret = 0;
        if (flags != newflags) {
                ret = fcntl(fd, F_SETFL, newflags);
                if (ret == -1) {
                        ret = errno;
                        assert(ret > 0);
                }
        }
        if (orig != NULL) {
                *orig = (flags & O_NONBLOCK) != 0;
        }
        return ret;
}

bool
is_again(int error)
{
        /* handle a BSD vs SYSV historical mess */
        return (error == EWOULDBLOCK || error == EAGAIN);
}

int
nbio_vfprintf(FILE *fp, const char *fmt, va_list ap)
{
        /*
         * XXX this implementation is effectively unbuffered.
         */

        char *buf;
        int ret;

        ret = vasprintf(&buf, fmt, ap);
        if (ret < 0) {
                return ret;
        }
        size_t len = ret;
        int fd = fileno(fp);

        flockfile(fp);
        ssize_t written = 0;
        while (written < len) {
                ssize_t ssz = write(fd, buf + written, len - written);
                assert(ssz != 0);
                if (ssz == -1) {
                        if (is_again(errno)) {
                                /*
                                 * probably the fd is non-blocking.
                                 * block with poll.
                                 */
                                struct pollfd pfd;
                                memset(&pfd, 0, sizeof(pfd));
                                pfd.fd = fd;
                                pfd.events = POLLOUT;
                                ret = poll(&pfd, 1, -1);
                                assert(ret != 0);
                                if (ret == 1) {
                                        continue;
                                }
                                assert(ret == -1);
                                if (errno == EAGAIN || errno == EINTR) {
                                        continue;
                                }
                                /* poll error, fall through */
                        }
                        /* errno is already set by write or poll */
                        written = -1;
                        assert(errno > 0);
                        break;
                }
                assert(0 < ssz && ssz <= len - written);
                written += ssz;
                assert(0 < written && written <= len);
        }
        int saved_errno = errno; /* just in case */
        funlockfile(fp);
        free(buf);
        assert(written == -1 || written == len);
        assert(written <= INT_MAX);
        assert(written >= INT_MIN);
        errno = saved_errno;
        return (int)written;
}

int
nbio_fprintf(FILE *fp, const char *fmt, ...)
{
        va_list ap;
        int ret;
        va_start(ap, fmt);
        ret = nbio_vfprintf(fp, fmt, ap);
        va_end(ap);
        return ret;
}

int
nbio_printf(const char *fmt, ...)
{
        va_list ap;
        int ret;
        va_start(ap, fmt);
        ret = nbio_vfprintf(stdout, fmt, ap);
        va_end(ap);
        return ret;
}

ssize_t
nbio_getline(char **linep, size_t *linecapp, FILE *fp)
{
        ssize_t ssz;
        int ret;
        int fd;
        bool orig;

        /*
         * Note: this relaxed implementation is ok, given our usage in repl.
         */
        fd = fileno(fp);
        ret = set_nonblocking(fd, false, &orig);
        if (ret != 0) {
                errno = ret;
                return -1;
        }
        ssz = getline(linep, linecapp, fp);
        ret = set_nonblocking(fd, orig, NULL);
        assert(ret == 0); /* no good way to recover */
        return ssz;
}
