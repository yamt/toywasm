#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "endian.h"
#include "exec.h"
#include "nbio.h"
#include "wasi_impl.h"
#include "wasi_path_subr.h"
#include "wasi_subr.h"
#include "wasi_utimes.h"
#include "wasi_vfs.h"
#include "xlog.h"

#include "wasi_hostfuncs.h"

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
        if ((wasmoflags & WASI_OFLAG_DIRECTORY) != 0 &&
            (rights_base & WASI_RIGHT_FD_WRITE) != 0) {
                ret = EISDIR;
                goto fail;
        }
        host_ret = wasi_copyin_and_convert_path(ctx, wasi, dirwasifd, path,
                                                pathlen, &pi, &ret);
        if (host_ret != 0 || ret != 0) {
                goto fail;
        }
        ret = wasi_vfs_path_fdinfo_alloc(&pi, &fdinfo);
        if (ret != 0) {
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
        ret = wasi_vfs_path_open(&pi, &open_params, fdinfo);
        if (ret != 0) {
                xlog_trace("open %s failed with %d", pi.hostpath, ret);
                goto fail;
        }
        uint32_t wasifd;
        ret = wasi_table_fdinfo_add(wasi, WASI_TABLE_FILES, fdinfo, &wasifd);
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
        path_clear(wasi, &pi);
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
        ret = wasi_vfs_path_unlink(&pi);
        if (ret != 0) {
                goto fail;
        }
fail:
        path_clear(wasi, &pi);
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
        ret = wasi_vfs_path_mkdir(&pi);
fail:
        path_clear(wasi, &pi);
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
        ret = wasi_vfs_path_rmdir(&pi);
fail:
        path_clear(wasi, &pi);
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
        ret = wasi_vfs_path_symlink(target_buf, &pi);
fail:
        free(target_buf);
        path_clear(wasi, &pi);
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
        host_ret = host_func_memory_getptr(ctx, 0, buf, 0, buflen, &p);
        if (host_ret != 0) {
                goto fail;
        }
        size_t n;
        ret = wasi_vfs_path_readlink(&pi, p, buflen, &n);
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
        path_clear(wasi, &pi);
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
        ret = wasi_vfs_path_link(&pi1, &pi2);
fail:
        path_clear(wasi, &pi1);
        path_clear(wasi, &pi2);
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
        ret = wasi_vfs_path_rename(&pi1, &pi2);
fail:
        path_clear(wasi, &pi1);
        path_clear(wasi, &pi2);
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
                ret = wasi_vfs_path_stat(&pi, &wst);
        } else {
                ret = wasi_vfs_path_lstat(&pi, &wst);
        }
        if (ret != 0) {
                goto fail;
        }
        host_ret = wasi_copyout(ctx, &wst, retp, sizeof(wst),
                                WASI_FILESTAT_ALIGN);
fail:
        path_clear(wasi, &pi);
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
                ret = wasi_vfs_path_stat(&pi, &wst);
        } else {
                ret = wasi_vfs_path_lstat(&pi, &wst);
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
        path_clear(wasi, &pi);
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
                ret = wasi_vfs_path_utimes(&pi, &args);
        } else {
                ret = wasi_vfs_path_lutimes(&pi, &args);
        }
fail:
        path_clear(wasi, &pi);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}
