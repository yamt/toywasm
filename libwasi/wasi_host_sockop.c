#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#define _NETBSD_SOURCE

#include <sys/socket.h>
#include <sys/uio.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h> /* for TOYWASM_OLD_WASI_LIBC check below */
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nbio.h"
#include "wasi_host_sockop.h"
#include "wasi_host_subr.h"
#include "wasi_impl.h"
#include "wasi_vfs_impl_host.h"

#if defined(__wasi__)
#if !defined(AT_FDCWD)
/* a workaroud for wasi-sdk-8.0 which we use for wapm */
#define TOYWASM_OLD_WASI_LIBC
#endif
#endif

int
wasi_host_sock_fdinfo_alloc(struct wasi_fdinfo *fdinfo,
                            struct wasi_fdinfo **fdinfop)
{
        return wasi_fdinfo_alloc_host(fdinfop);
}

int
wasi_host_sock_accept(struct wasi_fdinfo *fdinfo, uint16_t fdflags,
                      struct wasi_fdinfo *fdinfo2)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
        int hostchildfd = -1;
        int ret;
#if !defined(TOYWASM_OLD_WASI_LIBC)
        struct sockaddr_storage ss;
        struct sockaddr *sa = (void *)&ss;
        socklen_t salen;
#endif
#if defined(TOYWASM_OLD_WASI_LIBC)
        errno = ENOSYS;
        hostchildfd = -1;
#else
        hostchildfd = accept(hostfd, sa, &salen);
#endif
        if (hostchildfd < 0) {
                ret = errno;
                assert(ret > 0);
                return ret;
        }
        /*
         * Ensure O_NONBLOCK of the new socket.
         * Note: O_NONBLOCK behavior is not consistent among platforms.
         * eg. BSD inherits O_NONBLOCK. Linux doesn't.
         */
        ret = set_nonblocking(hostchildfd, true, NULL);
        if (ret != 0) {
                goto fail;
        }
        fdinfo->type = WASI_FDINFO_USER;
        struct wasi_fdinfo_host *fdinfo_host = wasi_fdinfo_to_host(fdinfo2);
        fdinfo_host->user.path = NULL;
        fdinfo_host->user.blocking = (fdflags & WASI_FDFLAG_NONBLOCK) == 0;
        fdinfo_host->hostfd = hostchildfd;
        fdinfo_host->dir = NULL;
        hostchildfd = -1;
        ret = 0;
fail:
        if (hostchildfd != -1) {
                close(hostchildfd);
        }
        return ret;
}

int
wasi_host_sock_recv(struct wasi_fdinfo *fdinfo, struct iovec *iov, int iovcnt,
                    uint16_t riflags, uint16_t *roflagsp, size_t *result)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
        int ret;
        int flags = 0;
        if ((riflags & WASI_RIFLAG_RECV_PEEK) != 0) {
                flags |= MSG_PEEK;
        }
        ssize_t n;
        struct msghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = iov;
        msg.msg_iovlen = iovcnt;
#if defined(__wasi__)
        (void)flags;
        n = -1;
        errno = ENOSYS;
#else
        n = recvmsg(hostfd, &msg, flags);
#endif
        if (n == -1) {
                ret = errno;
                assert(ret > 0);
                return ret;
        }
        uint16_t roflags = 0;
        /*
         * wasi-sdk <=19 had a broken MSG_TRUNC definition.
         * https://github.com/WebAssembly/wasi-libc/pull/391
         *
         * Note: older versions of wasi-sdk doesn't even have
         * __WASI_ROFLAGS_RECV_DATA_TRUNCATED.
         */
#if defined(__wasi__) && defined(MSG_TRUNC) &&                                \
        defined(__WASI_ROFLAGS_RECV_DATA_TRUNCATED)
#undef MSG_TRUNC
#define MSG_TRUNC __WASI_ROFLAGS_RECV_DATA_TRUNCATED
#endif /* defined(__wasi__) */
        if ((msg.msg_flags & MSG_TRUNC) != 0) {
                roflags = WASI_ROFLAG_RECV_DATA_TRUNCATED;
        }
        *roflagsp = roflags;
        *result = (size_t)n;
        return 0;
}

int
wasi_host_sock_send(struct wasi_fdinfo *fdinfo, struct iovec *iov, int iovcnt,
                    uint16_t siflags, size_t *result)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
        int ret;
        ssize_t n;
        struct msghdr msg;

        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = iov;
        msg.msg_iovlen = iovcnt;
#if defined(__wasi__)
        n = -1;
        errno = ENOSYS;
#else
        n = sendmsg(hostfd, &msg, 0);
#endif
        if (n == -1) {
                ret = errno;
                assert(ret > 0);
                return ret;
        }
        *result = (size_t)n;
        return 0;
}

int
wasi_host_sock_shutdown(struct wasi_fdinfo *fdinfo, uint16_t sdflags)
{
        int hostfd = wasi_fdinfo_hostfd(fdinfo);
        int ret;

        int how = 0;
        switch (sdflags) {
        case WASI_SDFLAG_RD | WASI_SDFLAG_WR:
                how = SHUT_RDWR;
                break;
        case WASI_SDFLAG_RD:
                how = SHUT_RD;
                break;
        case WASI_SDFLAG_WR:
                how = SHUT_WR;
                break;
        default:
                ret = EINVAL;
                goto fail;
        }
        ret = shutdown(hostfd, how);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
        }
fail:
        return ret;
}
