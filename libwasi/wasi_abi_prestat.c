#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "endian.h"
#include "wasi_impl.h"
#include "xlog.h"

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
        const struct wasi_fdinfo_prestat *fdinfo_prestat =
                wasi_fdinfo_to_prestat(fdinfo);
        const char *prestat_path = fdinfo_prestat->prestat_path;
        if (fdinfo_prestat->wasm_path != NULL) {
                prestat_path = fdinfo_prestat->wasm_path;
        }
        st.dir_name_len = host_to_le32(strlen(prestat_path));
        host_ret = wasi_copyout(ctx, wasi_memory(wasi), &st, retp, sizeof(st),
                                WASI_PRESTAT_ALIGN);
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
        const struct wasi_fdinfo_prestat *fdinfo_prestat =
                wasi_fdinfo_to_prestat(fdinfo);
        const char *prestat_path = fdinfo_prestat->prestat_path;
        if (fdinfo_prestat->wasm_path != NULL) {
                prestat_path = fdinfo_prestat->wasm_path;
        }
        xlog_trace("wasm fd %" PRIu32 " is prestat %s", wasifd, prestat_path);
        size_t len = strlen(prestat_path);
        if (len > pathlen) {
                xlog_trace("path buffer too small %zu > %" PRIu32, len,
                           pathlen);
                ret = EINVAL;
                goto fail;
        }
        host_ret = wasi_copyout(ctx, wasi_memory(wasi), prestat_path, path,
                                len, 1);
fail:
        wasi_fdinfo_release(wasi, fdinfo);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}
