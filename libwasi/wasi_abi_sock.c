#define _NETBSD_SOURCE /* for old netbsd */

#include <sys/socket.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "endian.h"
#include "nbio.h"
#include "wasi_host_subr.h"
#include "wasi_impl.h"
#include "wasi_poll_subr.h"
#include "wasi_subr.h"
#include "xlog.h"

#include "wasi_hostfuncs.h"

#if defined(__wasi__)
#if !defined(AT_FDCWD)
/* a workaroud for wasi-sdk-8.0 which we use for wapm */
#define TOYWASM_OLD_WASI_LIBC
#endif
#endif

int
wasi_sock_accept(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t fdflags = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 2, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int hostchildfd = -1;
        int host_ret = 0;
        int ret;

        /*
         * non-zero fdflags is used by accept4.
         *
         * only WASI_FDFLAG_NONBLOCK makes sense for a socket.
         *
         * as wasi doesn't have close-on-exec, accept4 itself doesn't
         * make much sense. it merely saves an fcntl.
         * (for a threaded environment, atomicity of close-on-exec is
         * important to avoid descriptor leaks when other threads
         * performs exec. it's why accept4 has been invented in
         * the first place. other flags are not that important.
         * cf. https://www.austingroupbugs.net/view.php?id=411)
         */
        if ((fdflags & ~WASI_FDFLAG_NONBLOCK) != 0) {
                xlog_error("%s: unsupported fdflags %x", __func__, fdflags);
                ret = ENOTSUP;
                goto fail;
        }

        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        struct sockaddr_storage ss;
        struct sockaddr *sa = (void *)&ss;
        socklen_t salen;
retry:
#if defined(TOYWASM_OLD_WASI_LIBC)
        errno = ENOSYS;
        hostchildfd = -1;
#else
        hostchildfd = accept(hostfd, sa, &salen);
#endif
        if (hostchildfd < 0) {
                ret = errno;
                assert(ret > 0);
                if (emulate_blocking(ctx, fdinfo, POLLIN, ret, &host_ret,
                                     &ret)) {
                        goto retry;
                }
                goto fail;
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
        uint32_t wasichildfd;
        ret = wasi_hostfd_add(wasi, hostchildfd, NULL,
                              fdflags & WASI_FDFLAG_NONBLOCK, &wasichildfd);
        if (ret != 0) {
                goto fail;
        }
        hostchildfd = -1;
        uint32_t r = host_to_le32(wasichildfd);
        host_ret = wasi_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
        if (host_ret != 0) {
                /* XXX close wasichildfd? */
                goto fail;
        }
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (hostchildfd != -1) {
                close(hostchildfd);
        }
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_sock_recv(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct cell *params,
               struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t iov_addr = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t iov_count = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t riflags = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 4, i32);
        uint32_t roflagsp = HOST_FUNC_PARAM(ft, params, 5, i32);
        struct iovec *hostiov = NULL;
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int host_ret = 0;
        int ret;
        int flags = 0;
        if ((riflags & ~(WASI_RIFLAG_RECV_PEEK | WASI_RIFLAG_RECV_WAITALL)) !=
            0) {
                ret = EINVAL;
                goto fail;
        }
        if ((riflags & WASI_RIFLAG_RECV_PEEK) != 0) {
                flags |= MSG_PEEK;
        }
        if ((riflags & WASI_RIFLAG_RECV_WAITALL) != 0) {
                /*
                 * Note: it seems difficult (or impossible) to emulate
                 * blocking MSG_WAITALL behavior with a non-blocking
                 * underlying socket.
                 *
                 * Note: MSG_WAITALL behavior on a non-blocking socket
                 * varies among platforms.
                 * eg. BSD honors MSG_WAITALL. Linux ignores it.
                 */
                ret = ENOTSUP;
                goto fail;
        }
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        ssize_t n;
        struct msghdr msg;
retry:
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = hostiov;
        msg.msg_iovlen = iov_count;
#if defined(__wasi__)
        n = -1;
        errno = ENOSYS;
#else
        n = recvmsg(hostfd, &msg, flags);
#endif
        if (n == -1) {
                ret = errno;
                assert(ret > 0);
                if (emulate_blocking(ctx, fdinfo, POLLIN, ret, &host_ret,
                                     &ret)) {
                        goto retry;
                }
                goto fail;
        }
        if (n > UINT32_MAX) {
                ret = EOVERFLOW;
                goto fail;
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
        uint32_t r = host_to_le32(n);
        host_ret = wasi_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
        if (host_ret != 0) {
                goto fail;
        }
        uint16_t roflags_wasm = host_to_le16(roflags);
        host_ret = wasi_copyout(ctx, &roflags_wasm, roflagsp,
                                sizeof(roflags_wasm), WASI_U16_ALIGN);
        if (host_ret != 0) {
                goto fail;
        }
        ret = 0;
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        free(hostiov);
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_sock_send(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct cell *params,
               struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t iov_addr = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t iov_count = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t siflags = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 4, i32);
        struct iovec *hostiov = NULL;
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
        int host_ret = 0;
        int ret;
        if (siflags != 0) {
                ret = EINVAL;
                goto fail;
        }
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        ssize_t n;
        struct msghdr msg;
retry:
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = hostiov;
        msg.msg_iovlen = iov_count;
#if defined(__wasi__)
        n = -1;
        errno = ENOSYS;
#else
        n = sendmsg(hostfd, &msg, 0);
#endif
        if (n == -1) {
                ret = errno;
                assert(ret > 0);
                if (emulate_blocking(ctx, fdinfo, POLLOUT, ret, &host_ret,
                                     &ret)) {
                        goto retry;
                }
                goto fail;
        }
        if (n > UINT32_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        uint32_t r = host_to_le32(n);
        host_ret = wasi_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
        ret = 0;
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        free(hostiov);
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_sock_shutdown(struct exec_context *ctx, struct host_instance *hi,
                   const struct functype *ft, const struct cell *params,
                   struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t sdflags = HOST_FUNC_PARAM(ft, params, 1, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int hostfd;
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
        ret = wasi_hostfd_lookup(wasi, wasifd, &hostfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        ret = shutdown(hostfd, how);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
        }
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}
