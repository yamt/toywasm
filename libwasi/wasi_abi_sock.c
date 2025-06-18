#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "endian.h"
#include "wasi_impl.h"
#include "wasi_poll_subr.h"
#include "wasi_subr.h"
#include "wasi_vfs.h"
#include "xlog.h"

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
        struct wasi_fdinfo *fdinfo_child = NULL;
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
                xlog_error("%s: unsupported fdflags %" PRIx32, __func__,
                           fdflags);
                ret = ENOTSUP;
                goto fail;
        }
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_vfs_sock_fdinfo_alloc(fdinfo, &fdinfo_child);
        if (ret != 0) {
                goto fail;
        }
retry:
        ret = wasi_vfs_sock_accept(fdinfo, fdflags, fdinfo_child);
        if (ret != 0) {
                if (emulate_blocking(ctx, fdinfo, POLLIN, ret, &host_ret,
                                     &ret)) {
                        goto retry;
                }
                goto fail;
        }
        uint32_t wasichildfd;
        ret = wasi_table_fdinfo_add(wasi, WASI_TABLE_FILES, fdinfo_child,
                                    &wasichildfd);
        if (ret != 0) {
                goto fail;
        }
        fdinfo_child = NULL;
        uint32_t r = host_to_le32(wasichildfd);
        host_ret = wasi_copyout(ctx, wasi_memory(wasi), &r, retp, sizeof(r),
                                WASI_U32_ALIGN);
        if (host_ret != 0) {
                /* XXX close wasichildfd? */
                goto fail;
        }
fail:
        if (fdinfo_child != NULL) {
                wasi_fdinfo_free(fdinfo_child);
        }
        wasi_fdinfo_release(wasi, fdinfo);
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
        int host_ret = 0;
        int ret;
        if ((riflags & ~(WASI_RIFLAG_RECV_PEEK | WASI_RIFLAG_RECV_WAITALL)) !=
            0) {
                ret = EINVAL;
                goto fail;
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
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_iovec(ctx, wasi_memory(wasi), iov_addr,
                                     iov_count, &hostiov, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        uint16_t roflags;
        size_t n;
retry:
        ret = wasi_vfs_sock_recv(fdinfo, hostiov, iov_count, riflags, &roflags,
                                 &n);
        if (ret != 0) {
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
        uint32_t r = host_to_le32(n);
        host_ret = wasi_copyout(ctx, wasi_memory(wasi), &r, retp, sizeof(r),
                                WASI_U32_ALIGN);
        if (host_ret != 0) {
                goto fail;
        }
        uint16_t roflags_wasm = host_to_le16(roflags);
        host_ret =
                wasi_copyout(ctx, wasi_memory(wasi), &roflags_wasm, roflagsp,
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
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_iovec(ctx, wasi_memory(wasi), iov_addr,
                                     iov_count, &hostiov, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        size_t n;
retry:
        ret = wasi_vfs_sock_send(fdinfo, hostiov, iov_count, siflags, &n);
        if (ret != 0) {
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
        host_ret = wasi_copyout(ctx, wasi_memory(wasi), &r, retp, sizeof(r),
                                WASI_U32_ALIGN);
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
        int ret;
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_vfs_sock_shutdown(fdinfo, sdflags);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}
