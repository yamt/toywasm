#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "endian.h"
#include "nbio.h"
#include "wasi_impl.h"
#include "wasi_poll_subr.h"
#include "wasi_subr.h"
#include "wasi_utimes.h"
#include "wasi_vfs.h"
#include "xlog.h"

#include "wasi_hostfuncs.h"

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
        ret = wasi_vfs_fd_fallocate(fdinfo, offset, len);
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
        ret = wasi_vfs_fd_ftruncate(fdinfo, size);
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
        const enum wasi_table_idx tblidx = WASI_TABLE_FILES;

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
        host_ret = wasi_table_lookup_locked_for_close(ctx, wasi, tblidx,
                                                      wasifd, &fdinfo, &ret);
        if (host_ret != 0 || ret != 0) {
                toywasm_mutex_unlock(&wasi->lock);
                goto fail;
        }

        assert(fdinfo->refcount == 2);
        fdinfo->refcount--;
        struct wasi_fdinfo **slot = wasi_table_slot_ptr(wasi, tblidx, wasifd);
        assert(*slot == fdinfo);
        *slot = NULL;
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
        ret = wasi_vfs_fd_writev(fdinfo, hostiov, iov_count, &n);
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
        ret = wasi_vfs_fd_pwritev(fdinfo, hostiov, iov_count, offset, &n);
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
        uint16_t fflags;
        ret = wasi_vfs_fd_get_flags(fdinfo, &fflags);
        if (ret != 0) {
                goto fail;
        }
        if ((fflags & WASI_FDFLAG_NONBLOCK) == 0) {
                /*
                 * perform a poll first to avoid blocking in readv.
                 */
                ret = EAGAIN;
                goto tty_hack;
        }
retry:
        ret = wasi_vfs_fd_readv(fdinfo, hostiov, iov_count, &n);
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
        ret = wasi_vfs_fd_preadv(fdinfo, hostiov, iov_count, offset, &n);
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
         * wasi_vfs_dir_rewind/wasi_vfs_dir_seek/wasi_vfs_dir_read
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
                ret = wasi_vfs_dir_rewind(fdinfo);
                if (ret != 0) {
                        goto fail;
                }
        } else if (cookie > LONG_MAX) {
                ret = EINVAL;
                goto fail;
        } else {
                xlog_trace("fd_readdir: seekdir %" PRIu64, cookie);
                ret = wasi_vfs_dir_seek(fdinfo, cookie);
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
                ret = wasi_vfs_dir_read(fdinfo, &wde, &d_name, &eod);
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
        struct wasi_fdstat st;
        memset(&st, 0, sizeof(st));
        if (wasi_fdinfo_is_prestat(fdinfo)) {
                st.fs_filetype = WASI_FILETYPE_DIRECTORY;
        } else {
                struct wasi_filestat wst;
                ret = wasi_vfs_fd_fstat(fdinfo, &wst);
                if (ret != 0) {
                        goto fail;
                }
                st.fs_filetype = wst.type;
                uint16_t flags;
                ret = wasi_vfs_fd_get_flags(fdinfo, &flags);
                if (ret != 0) {
                        goto fail;
                }
                /*
                 * Note: (flags & WASI_FDFLAG_NONBLOCK) is the non-block
                 * flag of the underlying host descriptor, which is usually
                 * true in this implementation.
                 * fdinfo_user->blocking is what users of wasi care.
                 */
                if (!wasi_fdinfo_to_user(fdinfo)->blocking) {
                        st.fs_flags |= host_to_le16(WASI_FDFLAG_NONBLOCK);
                }
                st.fs_flags |= host_to_le16(flags & WASI_FDFLAG_APPEND);
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
        ret = wasi_userfd_lookup(wasi, wasifd, &fdinfo);
        if (ret != 0) {
                goto fail;
        }
        wasi_fdinfo_to_user(fdinfo)->blocking =
                ((fdflags & WASI_FDFLAG_NONBLOCK) == 0);
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
        ret = wasi_vfs_fd_lseek(fdinfo, offset, hostwhence, &off);
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
        ret = wasi_vfs_fd_lseek(fdinfo, offset, hostwhence, &off);
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
        ret = wasi_vfs_fd_lseek(fdinfo, 0, SEEK_CUR, &off);
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
        ret = wasi_vfs_fd_fsync(fdinfo);
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
        ret = wasi_vfs_fd_fdatasync(fdinfo);
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
        const enum wasi_table_idx tblidx = WASI_TABLE_FILES;

        toywasm_mutex_lock(&wasi->lock);

        /* ensure the table size is big enough */
        ret = wasi_table_expand(wasi, tblidx, wasifd_to);
        if (ret != 0) {
                goto fail_locked;
        }

        /* Note: we check "to" first because it can involve a restart */

        /*
         * check "to"
         *
         * Note: unlike dup2, for some thread-safety reasons, fd_renumber
         * requires the "to" be an open descriptor.
         */
        host_ret = wasi_table_lookup_locked_for_close(
                ctx, wasi, tblidx, wasifd_to, &fdinfo_to, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail_locked;
        }

        /* check "from" */
        ret = wasi_table_lookup_locked(wasi, tblidx, wasifd_from,
                                       &fdinfo_from);
        if (ret != 0) {
                goto fail_locked;
        }

        /* renumber */
        assert(fdinfo_to->refcount == 2);
        fdinfo_to->refcount--;
        struct wasi_fdinfo **to_slot =
                wasi_table_slot_ptr(wasi, tblidx, wasifd_to);
        assert(*to_slot = fdinfo_to);
        *to_slot = fdinfo_from;
        struct wasi_fdinfo **from_slot =
                wasi_table_slot_ptr(wasi, tblidx, wasifd_from);
        assert(*from_slot = fdinfo_from);
        *from_slot = NULL;

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
        ret = wasi_vfs_fd_fstat(fdinfo, &wst);
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
        ret = wasi_vfs_fd_fstat(fdinfo, &wst);
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
        ret = wasi_vfs_fd_futimes(fdinfo, &args);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}
