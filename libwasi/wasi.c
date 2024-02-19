/*
 * WASI implementation for toywasm
 *
 * This is a bit relaxed implementation of WASI snapshot preview1.
 *
 * - The "rights" stuff is not implemented. mendokusai.
 *   Also, it's being removed in preview2.
 *
 * - The "openat" family API is intentionally not used in favor
 *   of portability.
 *   Note: It makes this implementation considerably complex/incomplete
 *   in some places because WASI is basically a copy of the openat API
 *   family.
 *
 * References:
 * https://github.com/WebAssembly/WASI/tree/main/phases/snapshot/witx
 * https://github.com/WebAssembly/wasi-libc/blob/main/libc-bottom-half/headers/public/wasi/api.h
 */

#define _DARWIN_C_SOURCE /* arc4random_buf */
#define _GNU_SOURCE      /* asprintf, realpath, O_DIRECTORY */
#define _NETBSD_SOURCE   /* asprintf, DT_REG, etc */

#if defined(__wasi__) && (!defined(__clang_major__) || __clang_major__ < 17)
/*
 * wasi-libc bug workaround.
 * https://github.com/WebAssembly/wasi-libc/pull/375
 *
 * LLVM 17 complains when you undefine builtin macros like __STDC_VERSION__.
 * (-Wbuiltin-macro-redefined)
 * if you are using LLVM 17, you probably are using a recent enough version
 * of wasi-libc which doesn't require the workaround.
 */
#undef __STDC_VERSION__
#define __STDC_VERSION__ 201112L
#endif

#if defined(__NuttX__)
#include <nuttx/config.h>
#endif

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(__GLIBC__) || defined(__NuttX__)
#include <sys/random.h> /* getrandom */
#endif

#include "endian.h"
#include "exec.h"
#include "lock.h"
#include "nbio.h"
#include "restart.h"
#include "timeutil.h"
#include "type.h"
#include "util.h"
#include "vec.h"
#include "wasi.h"
#include "wasi_host_dirent.h"
#include "wasi_host_fdop.h"
#include "wasi_host_pathop.h"
#include "wasi_host_subr.h"
#include "wasi_impl.h"
#include "wasi_path_subr.h"
#include "wasi_poll_subr.h"
#include "wasi_subr.h"
#include "wasi_utimes.h"
#include "xlog.h"

#include "wasi_hostfuncs.h"

#if defined(__wasi__)
#if !defined(AT_FDCWD)
/* a workaroud for wasi-sdk-8.0 which we use for wapm */
#define TOYWASM_OLD_WASI_LIBC
#endif
#endif

int
wasi_fd_advise(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct cell *params,
               struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
#if 0
        uint64_t offset = HOST_FUNC_PARAM(ft, params, 1, i64);
        uint64_t len = HOST_FUNC_PARAM(ft, params, 2, i64);
        uint32_t adv = HOST_FUNC_PARAM(ft, params, 3, i32);
#endif
        struct wasi_fdinfo *fdinfo;
        int ret;
#if defined(__GNUC__) && !defined(__clang__)
        fdinfo = NULL;
#endif
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        ret = 0;
        /* no-op */
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}

int
wasi_fd_allocate(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint64_t offset = HOST_FUNC_PARAM(ft, params, 1, i64);
        uint64_t len = HOST_FUNC_PARAM(ft, params, 2, i64);
        struct wasi_fdinfo *fdinfo;
        int ret;
#if defined(__GNUC__) && !defined(__clang__)
        fdinfo = NULL;
#endif
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_host_fd_fallocate(fdinfo, offset, len);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}

int
wasi_fd_filestat_set_size(struct exec_context *ctx, struct host_instance *hi,
                          const struct functype *ft, const struct cell *params,
                          struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint64_t size = HOST_FUNC_PARAM(ft, params, 1, i64);
        struct wasi_fdinfo *fdinfo;
        int ret;
#if defined(__GNUC__) && !defined(__clang__)
        fdinfo = NULL;
#endif
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        /*
         * Note: at least on macOS, ftruncate on a directory returns EINVAL,
         * not EISDIR. POSIX doesn't list EISDIR for ftruncate either.
         */
        xlog_trace("ftruncate wasifd %" PRIu32 " size %" PRIu64, wasifd, size);
        ret = wasi_host_fd_ftruncate(fdinfo, size);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}

int
wasi_fd_close(struct exec_context *ctx, struct host_instance *hi,
              const struct functype *ft, const struct cell *params,
              struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int host_ret = 0;
        int ret;

        /*
         * we simply make fd_close block until other threads finish working
         * on the descriptor.
         *
         * note that we can't assume that the other threads will finish on
         * the descriptor soon. actually they might keep using the
         * descriptor very long or even forever. (poll, socket read, ...)
         *
         * when i have done some experiments with native pthreads
         * decades ago, the behavior on concurrent close() was not
         * very consistent among platforms. (Solaris/BSDs/Linux...)
         * some of them block close() as we do here.
         * others "interrupt" and make other non-close users fail.
         *
         * i guess portable applications should not rely on either
         * behaviors.
         */

        toywasm_mutex_lock(&wasi->lock);
        host_ret = wasi_fd_lookup_locked_for_close(ctx, wasi, wasifd, &fdinfo,
                                                   &ret);
        if (host_ret != 0 || ret != 0) {
                toywasm_mutex_unlock(&wasi->lock);
                goto fail;
        }
        if (wasi_fdinfo_unused(fdinfo)) {
                toywasm_mutex_unlock(&wasi->lock);
                ret = EBADF;
                goto fail;
        }

        assert(fdinfo->refcount == 2);
        fdinfo->refcount--;
        assert(VEC_ELEM(wasi->fdtable, wasifd) == fdinfo);
        VEC_ELEM(wasi->fdtable, wasifd) = NULL;
        toywasm_mutex_unlock(&wasi->lock);

        ret = wasi_fdinfo_close(fdinfo);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_fd_write(struct exec_context *ctx, struct host_instance *hi,
              const struct functype *ft, const struct cell *params,
              struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t iov_addr = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t iov_count = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 3, i32);
        struct iovec *hostiov = NULL;
        struct wasi_fdinfo *fdinfo = NULL;
        int host_ret = 0;
        int ret;
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        size_t n;
retry:
        ret = wasi_host_fd_writev(fdinfo, hostiov, iov_count, &n);
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
wasi_fd_pwrite(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct cell *params,
               struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t iov_addr = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t iov_count = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint64_t offset = HOST_FUNC_PARAM(ft, params, 3, i64);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 4, i32);
        struct iovec *hostiov = NULL;
        struct wasi_fdinfo *fdinfo = NULL;
        int host_ret = 0;
        int ret;
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        size_t n;
retry:
        ret = wasi_host_fd_pwritev(fdinfo, hostiov, iov_count, offset, &n);
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
wasi_fd_read(struct exec_context *ctx, struct host_instance *hi,
             const struct functype *ft, const struct cell *params,
             struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t iov_addr = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t iov_count = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 3, i32);
        struct iovec *hostiov = NULL;
        struct wasi_fdinfo *fdinfo = NULL;
        int host_ret = 0;
        int ret;
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        size_t n;

        /* hack for tty. see the comment in wasi_instance_create. */
        int fflag;
        ret = wasi_host_fd_fcntl(fdinfo, F_GETFL, 0, &fflag);
        if (ret != 0) {
                goto fail;
        }
        if ((fflag & O_NONBLOCK) == 0) {
                /*
                 * perform a poll first to avoid blocking in readv.
                 */
                ret = EAGAIN;
                goto tty_hack;
        }
retry:
        ret = wasi_host_fd_readv(fdinfo, hostiov, iov_count, &n);
        if (ret != 0) {
tty_hack:
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
wasi_fd_pread(struct exec_context *ctx, struct host_instance *hi,
              const struct functype *ft, const struct cell *params,
              struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t iov_addr = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t iov_count = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint64_t offset = HOST_FUNC_PARAM(ft, params, 3, i64);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 4, i32);
        struct iovec *hostiov = NULL;
        struct wasi_fdinfo *fdinfo = NULL;
        int host_ret = 0;
        int ret;
        if (iov_count > INT_MAX) {
                ret = EOVERFLOW;
                goto fail;
        }
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_iovec(ctx, iov_addr, iov_count, &hostiov, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        size_t n;
retry:
        ret = wasi_host_fd_preadv(fdinfo, hostiov, iov_count, offset, &n);
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
wasi_fd_readdir(struct exec_context *ctx, struct host_instance *hi,
                const struct functype *ft, const struct cell *params,
                struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t buf = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t buflen = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint64_t cookie = HOST_FUNC_PARAM(ft, params, 3, i64);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 4, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int host_ret = 0;
        int ret;
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail_unlocked;
        }
        /*
         * (ab)use wasi->lock to serialize the following
         * wasi_host_dir_rewind/wasi_host_dir_seek/wasi_host_dir_read
         * sequence. REVISIT: it can be a per-fdinfo lock.
         */
        toywasm_mutex_lock(&wasi->lock);
        assert(fdinfo->type == WASI_FDINFO_USER);
        if (cookie == WASI_DIRCOOKIE_START) {
                /*
                 * Note: rewinddir invalidates cookies.
                 * is it what WASI expects?
                 */
                xlog_trace("fd_readdir: rewinddir");
                ret = wasi_host_dir_rewind(fdinfo);
                if (ret != 0) {
                        goto fail;
                }
        } else if (cookie > LONG_MAX) {
                ret = EINVAL;
                goto fail;
        } else {
                xlog_trace("fd_readdir: seekdir %" PRIu64, cookie);
                ret = wasi_host_dir_seek(fdinfo, cookie);
                if (ret != 0) {
                        goto fail;
                }
        }
        uint32_t n = 0;
        while (true) {
                struct wasi_dirent wde;
                const uint8_t *d_name;
                memset(&wde, 0, sizeof(wde));
                bool eod;
                ret = wasi_host_dir_read(fdinfo, &wde, &d_name, &eod);
                if (ret != 0) {
                        goto fail;
                }
                if (eod) {
                        break;
                }
                if (buflen - n < sizeof(wde)) {
                        xlog_trace("fd_readdir: buffer full");
                        n = buflen; /* signal buffer full */
                        break;
                }
                /* it's ok to return unaligned structure */
                host_ret = wasi_copyout(ctx, &wde, buf, sizeof(wde), 1);
                if (host_ret != 0) {
                        goto fail;
                }
                buf += sizeof(wde);
                n += sizeof(wde);
                uint32_t namlen = le32_decode(&wde.d_namlen);
                if (buflen - n < namlen) {
                        xlog_trace("fd_readdir: buffer full");
                        n = buflen; /* signal buffer full */
                        break;
                }
                host_ret = wasi_copyout(ctx, d_name, buf, namlen, 1);
                if (host_ret != 0) {
                        goto fail;
                }
                buf += namlen;
                n += namlen;
        }
        toywasm_mutex_unlock(&wasi->lock);
        uint32_t r = host_to_le32(n);
        host_ret = wasi_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
fail_unlocked:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
fail:
        toywasm_mutex_unlock(&wasi->lock);
        goto fail_unlocked;
}

int
wasi_fd_fdstat_get(struct exec_context *ctx, struct host_instance *hi,
                   const struct functype *ft, const struct cell *params,
                   struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t stat_addr = HOST_FUNC_PARAM(ft, params, 1, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int host_ret = 0;
        int ret;
        ret = wasi_fd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        if (wasi_fdinfo_unused(fdinfo)) {
                ret = EBADF;
                goto fail;
        }
        struct wasi_fdstat st;
        memset(&st, 0, sizeof(st));
        if (wasi_fdinfo_is_prestat(fdinfo)) {
                st.fs_filetype = WASI_FILETYPE_DIRECTORY;
        } else {
                struct wasi_filestat wst;
                ret = wasi_host_fd_fstat(fdinfo, &wst);
                if (ret != 0) {
                        goto fail;
                }
                st.fs_filetype = wst.type;
                int flags;
                ret = wasi_host_fd_fcntl(fdinfo, F_GETFL, 0, &flags);
                if (ret != 0) {
                        goto fail;
                }
                if (!fdinfo->blocking) {
                        st.fs_flags |= WASI_FDFLAG_NONBLOCK;
                }
                if ((flags & O_APPEND) != 0) {
                        st.fs_flags |= WASI_FDFLAG_APPEND;
                }
        }

        /*
         * for some reasons, old libc (eg. the one from wasi-sdk 8)
         * seems to perform ENOTCAPABLE checks for preopens by itself,
         * looking at fs_rights_base.
         *
         * Note: some code (eg. wasmtime wasi testsuite)
         * passes fs_rights_base from fd_fdstat_get to path_open.
         * our path_open uses WASI_RIGHT_FD_READ and WASI_RIGHT_FD_WRITE
         * to decide O_RDONLY/O_WRITEONLY/O_RDWR. the underlying os
         * and/or filesystem might reject them with EISDIR if it's
         * a directory.
         *
         * Note: In WASI, directories are not seekable.
         */
        const uint64_t seek_rights = WASI_RIGHT_FD_SEEK | WASI_RIGHT_FD_TELL;
        const uint64_t path_rights =
                WASI_RIGHT_PATH_CREATE_DIRECTORY |
                WASI_RIGHT_PATH_CREATE_FILE | WASI_RIGHT_PATH_LINK_SOURCE |
                WASI_RIGHT_PATH_LINK_TARGET | WASI_RIGHT_PATH_OPEN |
                WASI_RIGHT_PATH_READLINK | WASI_RIGHT_PATH_RENAME_SOURCE |
                WASI_RIGHT_PATH_RENAME_TARGET | WASI_RIGHT_PATH_FILESTAT_GET |
                WASI_RIGHT_PATH_FILESTAT_SET_SIZE |
                WASI_RIGHT_PATH_FILESTAT_SET_TIMES | WASI_RIGHT_PATH_SYMLINK |
                WASI_RIGHT_PATH_REMOVE_DIRECTORY | WASI_RIGHT_PATH_UNLINK_FILE;
        const uint64_t sock_rights =
                WASI_RIGHT_SOCK_SHUTDOWN | WASI_RIGHT_SOCK_ACCEPT;
        const uint64_t regfile_rights = WASI_RIGHT_FD_READ |
                                        WASI_RIGHT_FD_WRITE |
                                        WASI_RIGHT_FD_FILESTAT_SET_SIZE;
        uint64_t rights = ~UINT64_C(0);
        switch (st.fs_filetype) {
        case WASI_FILETYPE_DIRECTORY:
                rights = ~(regfile_rights | seek_rights | sock_rights);
                break;
        case WASI_FILETYPE_CHARACTER_DEVICE:
                /*
                 * Note: SEEK/TELL bits are important because
                 * wasi-libc isatty() checks them.
                 */
                rights = ~(path_rights | seek_rights | sock_rights);
                break;
        case WASI_FILETYPE_REGULAR_FILE:
                rights = ~(path_rights | sock_rights);
                break;
        }
        st.fs_rights_base = host_to_le64(rights);

        /*
         * A hack to make wasm-on-wasm happier.
         *
         * wasi-libc clamps the request by itself using rights_inheriting.
         * https://github.com/WebAssembly/wasi-libc/blob/9d2f5a8242667ac659793b19163cbeec1e077e01/libc-bottom-half/cloudlibc/src/libc/fcntl/openat.c#L53-L69
         */
        st.fs_rights_inheriting = ~UINT64_C(0);

        host_ret = wasi_copyout(ctx, &st, stat_addr, sizeof(st),
                                WASI_FDSTAT_ALIGN);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_fd_fdstat_set_flags(struct exec_context *ctx, struct host_instance *hi,
                         const struct functype *ft, const struct cell *params,
                         struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t fdflags = HOST_FUNC_PARAM(ft, params, 1, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int ret;
        if ((fdflags & ~WASI_FDFLAG_NONBLOCK) != 0) {
                ret = ENOTSUP;
                goto fail;
        }
        ret = wasi_fd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        fdinfo->blocking = ((fdflags & WASI_FDFLAG_NONBLOCK) == 0);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}

int
wasi_fd_fdstat_set_rights(struct exec_context *ctx, struct host_instance *hi,
                          const struct functype *ft, const struct cell *params,
                          struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
#if 0
        uint64_t base = HOST_FUNC_PARAM(ft, params, 1, i64);
        uint64_t inheriting = HOST_FUNC_PARAM(ft, params, 2, i64);
#endif
        struct wasi_fdinfo *fdinfo = NULL;
        int ret;
        ret = wasi_fd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        /* TODO implement */
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}

int
wasi_fd_seek(struct exec_context *ctx, struct host_instance *hi,
             const struct functype *ft, const struct cell *params,
             struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        int64_t offset = (int64_t)HOST_FUNC_PARAM(ft, params, 1, i64);
        uint32_t whence = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 3, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int host_ret = 0;
        int ret;
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        int hostwhence;
        switch (whence) {
        case 0:
                hostwhence = SEEK_SET;
                break;
        case 1:
                hostwhence = SEEK_CUR;
                break;
        case 2:
                hostwhence = SEEK_END;
                break;
        default:
                ret = EINVAL;
                goto fail;
        }
        ret = wasi_userfd_reject_directory(fdinfo);
        if (ret != 0) {
                goto fail;
        }
        wasi_off_t off;
        ret = wasi_host_fd_lseek(fdinfo, offset, hostwhence, &off);
        if (ret != 0) {
                goto fail;
        }
        uint64_t result = host_to_le64(off);
        host_ret = wasi_copyout(ctx, &result, retp, sizeof(result),
                                WASI_U64_ALIGN);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_unstable_fd_seek(struct exec_context *ctx, struct host_instance *hi,
                      const struct functype *ft, const struct cell *params,
                      struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        int64_t offset = (int64_t)HOST_FUNC_PARAM(ft, params, 1, i64);
        uint32_t whence = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 3, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int host_ret = 0;
        int ret;
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        int hostwhence;
        switch (whence) {
        case 0:
                hostwhence = SEEK_CUR;
                break;
        case 1:
                hostwhence = SEEK_END;
                break;
        case 2:
                hostwhence = SEEK_SET;
                break;
        default:
                ret = EINVAL;
                goto fail;
        }
        ret = wasi_userfd_reject_directory(fdinfo);
        if (ret != 0) {
                goto fail;
        }
        wasi_off_t off;
        ret = wasi_host_fd_lseek(fdinfo, offset, hostwhence, &off);
        if (ret != 0) {
                goto fail;
        }
        uint64_t result = host_to_le64(off);
        host_ret = wasi_copyout(ctx, &result, retp, sizeof(result),
                                WASI_U64_ALIGN);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_fd_tell(struct exec_context *ctx, struct host_instance *hi,
             const struct functype *ft, const struct cell *params,
             struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 1, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int host_ret = 0;
        int ret;
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_userfd_reject_directory(fdinfo);
        if (ret != 0) {
                goto fail;
        }
        wasi_off_t off;
        ret = wasi_host_fd_lseek(fdinfo, 0, SEEK_CUR, &off);
        if (ret != 0) {
                goto fail;
        }
        uint64_t result = host_to_le64(off);
        host_ret = wasi_copyout(ctx, &result, retp, sizeof(result),
                                WASI_U64_ALIGN);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_fd_sync(struct exec_context *ctx, struct host_instance *hi,
             const struct functype *ft, const struct cell *params,
             struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int ret;
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_host_fd_fsync(fdinfo);
        if (ret != 0) {
                goto fail;
        }
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}

int
wasi_fd_datasync(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int ret;
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_host_fd_fdatasync(fdinfo);
        if (ret != 0) {
                goto fail;
        }
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}

int
wasi_fd_renumber(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd_from = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t wasifd_to = HOST_FUNC_PARAM(ft, params, 1, i32);
        struct wasi_fdinfo *fdinfo_from = NULL;
        struct wasi_fdinfo *fdinfo_to = NULL;
        int host_ret = 0;
        int ret;

        toywasm_mutex_lock(&wasi->lock);

        /* ensure the table size is big enough */
        ret = wasi_fdtable_expand(wasi, wasifd_to);
        if (ret != 0) {
                goto fail_locked;
        }

        /* Note: we check "to" first because it can involve a restart */

        /* check "to" */
        host_ret = wasi_fd_lookup_locked_for_close(ctx, wasi, wasifd_to,
                                                   &fdinfo_to, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail_locked;
        }
        /*
         * Note: unlike dup2, for some thread-safety reasons, fd_renumber
         * requires the "to" be an open descriptor.
         */
        if (wasi_fdinfo_unused(fdinfo_to)) {
                ret = EBADF;
                goto fail_locked;
        }

        /* check "from" */
        ret = wasi_fd_lookup_locked(wasi, wasifd_from, &fdinfo_from);
        if (ret != 0) {
                goto fail_locked;
        }
        if (fdinfo_from->type == WASI_FDINFO_USER &&
            fdinfo_from->u.u_user.hostfd == -1) {
                ret = EBADF;
                goto fail_locked;
        }

        /* renumber */
        assert(fdinfo_to->refcount == 2);
        fdinfo_to->refcount--;
        VEC_ELEM(wasi->fdtable, wasifd_to) = fdinfo_from;
        VEC_ELEM(wasi->fdtable, wasifd_from) = NULL;

        toywasm_mutex_unlock(&wasi->lock);

        wasi_fdinfo_release(wasi, fdinfo_from);
        fdinfo_from = NULL;

        /* close the old "to" file */
        ret = wasi_fdinfo_close(fdinfo_to);
        if (ret != 0) {
                /* log and ignore */
                xlog_error("%s: closing to-fd failed with %d", __func__, ret);
                ret = 0;
        }
fail:
        wasi_fdinfo_release(wasi, fdinfo_from);
        wasi_fdinfo_release(wasi, fdinfo_to);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
fail_locked:
        toywasm_mutex_unlock(&wasi->lock);
        goto fail;
}

int
wasi_fd_filestat_get(struct exec_context *ctx, struct host_instance *hi,
                     const struct functype *ft, const struct cell *params,
                     struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 1, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int host_ret = 0;
        int ret;
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        struct wasi_filestat wst;
        ret = wasi_host_fd_fstat(fdinfo, &wst);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyout(ctx, &wst, retp, sizeof(wst),
                                WASI_FILESTAT_ALIGN);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_unstable_fd_filestat_get(struct exec_context *ctx,
                              struct host_instance *hi,
                              const struct functype *ft,
                              const struct cell *params, struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 1, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int host_ret = 0;
        int ret;
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        struct wasi_filestat wst;
        ret = wasi_host_fd_fstat(fdinfo, &wst);
        if (ret != 0) {
                goto fail;
        }
        struct wasi_unstable_filestat uwst;
        ret = wasi_unstable_convert_filestat(&wst, &uwst);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyout(ctx, &uwst, retp, sizeof(uwst),
                                WASI_UNSTABLE_FILESTAT_ALIGN);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_fd_filestat_set_times(struct exec_context *ctx, struct host_instance *hi,
                           const struct functype *ft,
                           const struct cell *params, struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint64_t atim = HOST_FUNC_PARAM(ft, params, 1, i64);
        uint64_t mtim = HOST_FUNC_PARAM(ft, params, 2, i64);
        uint32_t fstflags = HOST_FUNC_PARAM(ft, params, 3, i32);
        struct wasi_fdinfo *fdinfo = NULL;
        int ret;
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        struct utimes_args args = {
                .fstflags = fstflags,
                .atim = atim,
                .mtim = mtim,
        };
        ret = wasi_host_fd_futimes(fdinfo, &args);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}

int
wasi_fd_prestat_get(struct exec_context *ctx, struct host_instance *hi,
                    const struct functype *ft, const struct cell *params,
                    struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 1, i32);
        int host_ret = 0;
        int ret;
        struct wasi_fdinfo *fdinfo = NULL;
        ret = wasi_fd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        if (!wasi_fdinfo_is_prestat(fdinfo)) {
                ret = EBADF;
                goto fail;
        }
        struct wasi_fd_prestat st;
        memset(&st, 0, sizeof(st));
        st.type = WASI_PREOPEN_TYPE_DIR;
        const char *prestat_path = fdinfo->u.u_prestat.prestat_path;
        if (fdinfo->u.u_prestat.wasm_path != NULL) {
                prestat_path = fdinfo->u.u_prestat.wasm_path;
        }
        st.dir_name_len = host_to_le32(strlen(prestat_path));
        host_ret =
                wasi_copyout(ctx, &st, retp, sizeof(st), WASI_PRESTAT_ALIGN);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_fd_prestat_dir_name(struct exec_context *ctx, struct host_instance *hi,
                         const struct functype *ft, const struct cell *params,
                         struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t wasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 2, i32);
        int host_ret = 0;
        int ret;
        struct wasi_fdinfo *fdinfo = NULL;
        ret = wasi_fd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        if (!wasi_fdinfo_is_prestat(fdinfo)) {
                xlog_trace("wasm fd %" PRIu32 " is not prestat", wasifd);
                ret = EBADF;
                goto fail;
        }
        const char *prestat_path = fdinfo->u.u_prestat.prestat_path;
        if (fdinfo->u.u_prestat.wasm_path != NULL) {
                prestat_path = fdinfo->u.u_prestat.wasm_path;
        }
        xlog_trace("wasm fd %" PRIu32 " is prestat %s", wasifd, prestat_path);
        size_t len = strlen(prestat_path);
        if (len > pathlen) {
                xlog_trace("path buffer too small %zu > %" PRIu32, len,
                           pathlen);
                ret = EINVAL;
                goto fail;
        }
        host_ret = wasi_copyout(ctx, prestat_path, path, len, 1);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_path_open(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct cell *params,
               struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t lookupflags = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t wasmoflags = HOST_FUNC_PARAM(ft, params, 4, i32);
        uint64_t rights_base = HOST_FUNC_PARAM(ft, params, 5, i64);
#if 0
        uint64_t rights_inherit = HOST_FUNC_PARAM(ft, params, 6, i64);
#endif
        uint32_t fdflags = HOST_FUNC_PARAM(ft, params, 7, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 8, i32);
        struct path_info pi = PATH_INITIALIZER;
        struct wasi_fdinfo *fdinfo = NULL;
        int host_ret = 0;
        int ret;
        xlog_trace("wasm oflags %" PRIx32 " rights_base %" PRIx64, wasmoflags,
                   rights_base);
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &pi, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        fdinfo = wasi_fdinfo_alloc();
        if (fdinfo == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        /*
         * TODO: avoid blocking on fifos for wasi-threads.
         */
        struct path_open_params open_params = {
                .lookupflags = lookupflags,
                .wasmoflags = wasmoflags,
                .rights_base = rights_base,
                .fdflags = fdflags,
        };
        ret = wasi_host_path_open(&pi, &open_params, fdinfo);
        if (ret != 0) {
                xlog_trace("open %s failed with %d", pi.hostpath, ret);
                goto fail;
        }
        uint32_t wasifd;
        ret = wasi_fdinfo_add(wasi, fdinfo, &wasifd);
        if (ret != 0) {
                goto fail;
        }
        fdinfo = NULL;
        xlog_trace("-> new wasi fd %" PRIu32, wasifd);
        uint32_t r = host_to_le32(wasifd);
        host_ret = wasi_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
        if (host_ret != 0) {
                /* XXX close wasifd? */
                goto fail;
        }
fail:
        if (fdinfo != NULL) {
                wasi_fdinfo_free(fdinfo);
        }
        path_clear(&pi);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_path_unlink_file(struct exec_context *ctx, struct host_instance *hi,
                      const struct functype *ft, const struct cell *params,
                      struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t pathstr = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 2, i32);
        struct path_info pi = PATH_INITIALIZER;
        int host_ret;
        int ret = 0;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, pathstr,
                                                pathlen, &pi, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        ret = wasi_host_path_unlink(&pi);
        if (ret != 0) {
                goto fail;
        }
fail:
        path_clear(&pi);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_path_create_directory(struct exec_context *ctx, struct host_instance *hi,
                           const struct functype *ft,
                           const struct cell *params, struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 2, i32);
        int host_ret;
        int ret = 0;
        struct path_info pi;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &pi, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        ret = wasi_host_path_mkdir(&pi);
fail:
        path_clear(&pi);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_path_remove_directory(struct exec_context *ctx, struct host_instance *hi,
                           const struct functype *ft,
                           const struct cell *params, struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 2, i32);
        int host_ret;
        int ret = 0;
        struct path_info pi = PATH_INITIALIZER;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &pi, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        ret = wasi_host_path_rmdir(&pi);
fail:
        path_clear(&pi);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_path_symlink(struct exec_context *ctx, struct host_instance *hi,
                  const struct functype *ft, const struct cell *params,
                  struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t target = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t targetlen = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 4, i32);
        char *target_buf;
        struct path_info pi = PATH_INITIALIZER;
        int host_ret = 0;
        int ret = 0;
        target_buf = malloc(targetlen + 1);
        if (target_buf == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        host_ret = wasi_copyin(ctx, target_buf, target, targetlen, 1);
        if (host_ret != 0) {
                goto fail;
        }
        target_buf[targetlen] = 0;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &pi, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        ret = wasi_host_path_symlink(target_buf, &pi);
fail:
        free(target_buf);
        path_clear(&pi);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_path_readlink(struct exec_context *ctx, struct host_instance *hi,
                   const struct functype *ft, const struct cell *params,
                   struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t buf = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t buflen = HOST_FUNC_PARAM(ft, params, 4, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 5, i32);
        struct path_info pi = PATH_INITIALIZER;
        int host_ret = 0;
        int ret = 0;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &pi, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        /*
         * Traditionaly, readlink with an insufficiant buffer size
         * silently truncates the contents.
         * It's also what POSIX requires:
         * > If the buf argument is not large enough to contain
         * > the link content, the first bufsize bytes shall be
         * > placed in buf.
         *
         * For some reasons, wasmtime used to return ERANGE.
         * https://github.com/bytecodealliance/wasmtime/commit/222a57868e9c01baa838aa81e92a80451e2d920a
         * However, it has been fixed.
         * https://github.com/bytecodealliance/wasmtime/commit/24b607cf751930c51f2b6449cdfbf2e81dce1c31
         */
        void *p;
        host_ret = memory_getptr(ctx, 0, buf, 0, buflen, &p);
        if (host_ret != 0) {
                goto fail;
        }
        size_t n;
        ret = wasi_host_path_readlink(&pi, p, buflen, &n);
        if (ret != 0) {
                goto fail;
        }
        uint32_t result = le32_to_host(n);
        host_ret = wasi_copyout(ctx, &result, retp, sizeof(result),
                                WASI_U32_ALIGN);
        if (host_ret != 0) {
                goto fail;
        }
fail:
        path_clear(&pi);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_path_link(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct cell *params,
               struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd1 = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t lookupflags = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t path1 = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t pathlen1 = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t dirwasifd2 = HOST_FUNC_PARAM(ft, params, 4, i32);
        uint32_t path2 = HOST_FUNC_PARAM(ft, params, 5, i32);
        uint32_t pathlen2 = HOST_FUNC_PARAM(ft, params, 6, i32);
        struct path_info pi1 = PATH_INITIALIZER;
        struct path_info pi2 = PATH_INITIALIZER;
        int host_ret = 0;
        int ret = 0;
        if ((lookupflags & WASI_LOOKUPFLAG_SYMLINK_FOLLOW) == 0) {
#if 1
                ret = ENOTSUP;
                goto fail;
#else
                xlog_trace(
                        "path_link: Ignoring !WASI_LOOKUPFLAG_SYMLINK_FOLLOW");
#endif
        }
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd1, path1,
                                                pathlen1, &pi1, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd2, path2,
                                                pathlen2, &pi2, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        ret = wasi_host_path_link(&pi1, &pi2);
fail:
        path_clear(&pi1);
        path_clear(&pi2);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_path_rename(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd1 = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t path1 = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t pathlen1 = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t dirwasifd2 = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t path2 = HOST_FUNC_PARAM(ft, params, 4, i32);
        uint32_t pathlen2 = HOST_FUNC_PARAM(ft, params, 5, i32);
        struct path_info pi1 = PATH_INITIALIZER;
        struct path_info pi2 = PATH_INITIALIZER;
        int host_ret;
        int ret = 0;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd1, path1,
                                                pathlen1, &pi1, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd2, path2,
                                                pathlen2, &pi2, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        ret = wasi_host_path_rename(&pi1, &pi2);
fail:
        path_clear(&pi1);
        path_clear(&pi2);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_path_filestat_get(struct exec_context *ctx, struct host_instance *hi,
                       const struct functype *ft, const struct cell *params,
                       struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t lookupflags = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 4, i32);
        struct path_info pi = PATH_INITIALIZER;
        int host_ret = 0;
        int ret;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &pi, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        struct wasi_filestat wst;
        if ((lookupflags & WASI_LOOKUPFLAG_SYMLINK_FOLLOW) != 0) {
                ret = wasi_host_path_stat(&pi, &wst);
        } else {
                ret = wasi_host_path_lstat(&pi, &wst);
        }
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyout(ctx, &wst, retp, sizeof(wst),
                                WASI_FILESTAT_ALIGN);
fail:
        path_clear(&pi);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_unstable_path_filestat_get(struct exec_context *ctx,
                                struct host_instance *hi,
                                const struct functype *ft,
                                const struct cell *params,
                                struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t lookupflags = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 4, i32);
        struct path_info pi = PATH_INITIALIZER;
        int host_ret = 0;
        int ret;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &pi, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        struct wasi_filestat wst;
        if ((lookupflags & WASI_LOOKUPFLAG_SYMLINK_FOLLOW) != 0) {
                ret = wasi_host_path_stat(&pi, &wst);
        } else {
                ret = wasi_host_path_lstat(&pi, &wst);
        }
        if (ret != 0) {
                goto fail;
        }
        struct wasi_unstable_filestat uwst;
        ret = wasi_unstable_convert_filestat(&wst, &uwst);
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyout(ctx, &uwst, retp, sizeof(uwst),
                                WASI_UNSTABLE_FILESTAT_ALIGN);
fail:
        path_clear(&pi);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_path_filestat_set_times(struct exec_context *ctx,
                             struct host_instance *hi,
                             const struct functype *ft,
                             const struct cell *params, struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t dirwasifd = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t lookupflags = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t path = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t pathlen = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint64_t atim = HOST_FUNC_PARAM(ft, params, 4, i64);
        uint64_t mtim = HOST_FUNC_PARAM(ft, params, 5, i64);
        uint32_t fstflags = HOST_FUNC_PARAM(ft, params, 6, i32);
        struct path_info pi = PATH_INITIALIZER;
        int host_ret;
        int ret = 0;
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &pi, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        struct utimes_args args = {
                .fstflags = fstflags,
                .atim = atim,
                .mtim = mtim,
        };
        if ((lookupflags & WASI_LOOKUPFLAG_SYMLINK_FOLLOW) != 0) {
                ret = wasi_host_path_utimes(&pi, &args);
        } else {
                ret = wasi_host_path_lutimes(&pi, &args);
        }
fail:
        path_clear(&pi);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

#define WASI_API(a, b) WASI_HOST_FUNC(a, b),
#define WASI_API2(a, b, c) WASI_HOST_FUNC2(a, b, c),
const struct host_func wasi_funcs[] = {
#include "wasi_preview1.h"
};

/*
 * a few incompatibilities between wasi_unstable and
 * wasi_snapshot_preview1:
 *
 * |                | unstable | preview1 |
 * |----------------|----------|----------|
 * | SEEK_CUR       | 0        | 1        |
 * | SEEK_END       | 1        | 2        |
 * | SEEK_SET       | 2        | 0        |
 * | filestat nlink | 32-bit   | 64-bit   |
 */
const struct host_func wasi_unstable_funcs[] = {
#include "wasi_unstable.h"
};
#undef WASI_API
#undef WASI_API2

int
wasi_instance_add_hostfd(struct wasi_instance *inst, uint32_t wasmfd,
                         int hostfd)
{
        struct wasi_fdinfo *fdinfo = NULL;
        int ret;
        toywasm_mutex_lock(&inst->lock);
        ret = wasi_fdtable_expand(inst, wasmfd);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_fd_lookup_locked(inst, wasmfd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        if (!wasi_fdinfo_unused(fdinfo)) {
                ret = EBUSY;
                goto fail;
        }

        /*
         * we make hostfds non-blocking.
         *
         * XXX should restore on failure
         * XXX should restore when we are done
         * XXX this affects other programs sharing files.
         *     (eg. shell pipelines)
         *
         * a fragile hack:
         *
         * tty is often shared with other processes.
         * making such files non blocking breaks other
         * processes.
         * eg. when you run a shell command like
         * "toywasm | more", the tty is toywasm's stdin
         * and also more's stdout.
         *
         * IMO, it's a design mistake (or at least a compromise)
         * to control non-blocking with fcntl(). It should be
         * a per-operation flag instead. eg. MSG_DONTWAIT.
         * Unfortunately, not all operations/platforms have
         * flags like that.
         */
        if (!isatty(hostfd)) {
                ret = set_nonblocking(hostfd, true, NULL);
                if (ret != 0) {
                        xlog_error("set_nonblocking failed on fd %d with %d",
                                   hostfd, ret);
                        goto fail;
                }
        }

        int dupfd;
#if defined(__wasi__) /* wasi has no dup */
        dupfd = hostfd;
#else
        dupfd = dup(hostfd);
#endif
        fdinfo->type = WASI_FDINFO_USER;
        fdinfo->u.u_user.hostfd = dupfd;
        if (dupfd == -1) {
                xlog_trace("failed to dup: wasm fd %" PRIu32
                           " host fd %u with errno %d",
                           wasmfd, hostfd, errno);
        }
        ret = 0;
fail:
        toywasm_mutex_unlock(&inst->lock);
        wasi_fdinfo_release(inst, fdinfo);
        return ret;
}

int
wasi_instance_populate_stdio_with_hostfd(struct wasi_instance *inst)
{
        uint32_t nfds = 3;
        uint32_t i;
        int ret;
        for (i = 0; i < nfds; i++) {
                ret = wasi_instance_add_hostfd(inst, i, i);
                if (ret != 0) {
                        xlog_error("wasi_instance_add_hostfd failed on fd %d "
                                   "with %d",
                                   i, ret);
                        goto fail;
                }
        }
        ret = 0;
fail:
        return ret;
}

int
wasi_instance_create(struct wasi_instance **instp) NO_THREAD_SAFETY_ANALYSIS
{
        struct wasi_instance *inst;

        inst = zalloc(sizeof(*inst));
        if (inst == NULL) {
                return ENOMEM;
        }
        toywasm_mutex_init(&inst->lock);
        toywasm_cv_init(&inst->cv);
        *instp = inst;
        return 0;
}

void
wasi_instance_set_args(struct wasi_instance *inst, int argc,
                       const char *const *argv)
{
        inst->argc = argc;
        inst->argv = argv;
#if defined(TOYWASM_ENABLE_TRACING)
        xlog_trace("%s argc = %u", __func__, argc);
        int i;
        for (i = 0; i < argc; i++) {
                xlog_trace("%s arg[%u] = \"%s\"", __func__, i, argv[i]);
        }
#endif
}

void
wasi_instance_set_environ(struct wasi_instance *inst, int nenvs,
                          const char *const *envs)
{
        inst->nenvs = nenvs;
        inst->envs = envs;
#if defined(TOYWASM_ENABLE_TRACING)
        xlog_trace("%s nenvs = %u", __func__, nenvs);
        int i;
        for (i = 0; i < nenvs; i++) {
                xlog_trace("%s env[%u] = \"%s\"", __func__, i, envs[i]);
        }
#endif
}

static int
wasi_instance_prestat_add_common(struct wasi_instance *wasi, const char *path,
                                 bool is_mapdir)
{
        struct wasi_fdinfo *fdinfo = NULL;
        char *host_path = NULL;
        char *wasm_path = NULL;
        uint32_t wasifd;
        int ret;
        xlog_trace("prestat adding mapdir %s", path);

        if (is_mapdir) {
                /*
                 * <wasm dir>::<host dir>
                 *
                 * intended to be compatible with wasmtime's --mapdir
                 */

                const char *colon = strchr(path, ':');
                if (colon == NULL || colon[1] != ':') {
                        ret = EINVAL;
                        goto fail;
                }
                wasm_path = strndup(path, colon - path);
                host_path = strdup(colon + 2);
        } else {
                host_path = strdup(path);
        }
        fdinfo = wasi_fdinfo_alloc();
        if (host_path == NULL || (is_mapdir && wasm_path == NULL) ||
            fdinfo == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        fdinfo->type = WASI_FDINFO_PRESTAT;
        fdinfo->u.u_prestat.prestat_path = host_path;
        fdinfo->u.u_prestat.wasm_path = wasm_path;
        host_path = NULL;
        wasm_path = NULL;
        ret = wasi_fdinfo_add(wasi, fdinfo, &wasifd);
        if (ret != 0) {
                goto fail;
        }
        xlog_trace("prestat added %s (%s)", path,
                   fdinfo->u.u_prestat.prestat_path);
        return 0;
fail:
        free(host_path);
        free(wasm_path);
        wasi_fdinfo_free(fdinfo);
        return ret;
}

int
wasi_instance_prestat_add(struct wasi_instance *wasi, const char *path)
{
        return wasi_instance_prestat_add_common(wasi, path, false);
}

int
wasi_instance_prestat_add_mapdir(struct wasi_instance *wasi, const char *path)
{
        return wasi_instance_prestat_add_common(wasi, path, true);
}

uint32_t
wasi_instance_exit_code(struct wasi_instance *wasi)
{
        uint32_t exit_code;
        toywasm_mutex_lock(&wasi->lock);
        exit_code = wasi->exit_code;
        toywasm_mutex_unlock(&wasi->lock);
        return exit_code;
}

void
wasi_instance_destroy(struct wasi_instance *inst)
{
        wasi_fdtable_free(inst);
        toywasm_cv_destroy(&inst->cv);
        toywasm_mutex_destroy(&inst->lock);
        free(inst);
}

static const struct name wasi_snapshot_preview1 =
        NAME_FROM_CSTR_LITERAL("wasi_snapshot_preview1");

static const struct name wasi_unstable =
        NAME_FROM_CSTR_LITERAL("wasi_unstable");

static const struct host_module module_wasi[] = {
        {
                .module_name = &wasi_snapshot_preview1,
                .funcs = wasi_funcs,
                .nfuncs = ARRAYCOUNT(wasi_funcs),
        },
        {
                .module_name = &wasi_unstable,
                .funcs = wasi_unstable_funcs,
                .nfuncs = ARRAYCOUNT(wasi_unstable_funcs),
        },
        {
                .module_name = &wasi_unstable,
                .funcs = wasi_funcs,
                .nfuncs = ARRAYCOUNT(wasi_funcs),
        },
};

int
import_object_create_for_wasi(struct wasi_instance *wasi,
                              struct import_object **impp)
{
        return import_object_create_for_host_funcs(
                module_wasi, ARRAYCOUNT(module_wasi), &wasi->hi, impp);
}
