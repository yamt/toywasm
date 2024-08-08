#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "endian.h"
#include "wasi_impl.h"
#include "xlog.h"

#include "wasi_hostfuncs.h"

static int
args_environ_sizes_get(struct exec_context *ctx, struct wasi_instance *wasi,
                       const struct functype *ft, const struct cell *params,
                       struct cell *results, int argc, const char *const *argv)
{
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t argcp = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t argv_buf_sizep = HOST_FUNC_PARAM(ft, params, 1, i32);
        int host_ret;
        uint32_t argc_le32 = host_to_le32(argc);
        host_ret = wasi_copyout(ctx, wasi_memory(wasi), &argc_le32, argcp,
                                sizeof(argc_le32), WASI_U32_ALIGN);
        if (host_ret != 0) {
                goto fail;
        }
        int i;
        uint32_t argv_buf_size = 0;
        for (i = 0; i < argc; i++) {
                argv_buf_size += strlen(argv[i]) + 1;
        }
        argv_buf_size = host_to_le32(argv_buf_size);
        host_ret = wasi_copyout(ctx, wasi_memory(wasi), &argv_buf_size,
                                argv_buf_sizep, sizeof(argv_buf_size), 1);
fail:
        if (host_ret == 0) {
                int ret = 0; /* never fail */
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static int
args_environ_get(struct exec_context *ctx, struct wasi_instance *wasi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results, int argc, const char *const *argv)
{
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t argvp = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t argv_buf = HOST_FUNC_PARAM(ft, params, 1, i32);
        int host_ret = 0;
        int ret = 0;
        uint32_t i;
        uint32_t *wasm_argv = NULL;
        if (argc > 0) {
                wasm_argv = malloc(argc * sizeof(*wasm_argv));
                if (wasm_argv == NULL) {
                        ret = ENOMEM;
                        goto fail;
                }
        }
        uint32_t wasmp = argv_buf;
        for (i = 0; i < argc; i++) {
                le32_encode(&wasm_argv[i], wasmp);
                xlog_trace("wasm_argv[%" PRIu32 "] %" PRIx32, i, wasmp);
                wasmp += strlen(argv[i]) + 1;
        }
        for (i = 0; i < argc; i++) {
                size_t sz = strlen(argv[i]) + 1;
                host_ret = wasi_copyout(ctx, wasi_memory(wasi), argv[i],
                                        le32_to_host(wasm_argv[i]), sz, 1);
                if (host_ret != 0) {
                        goto fail;
                }
        }
        host_ret = wasi_copyout(ctx, wasi_memory(wasi), wasm_argv, argvp,
                                argc * sizeof(*wasm_argv), WASI_U32_ALIGN);
fail:
        free(wasm_argv);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_args_sizes_get(struct exec_context *ctx, struct host_instance *hi,
                    const struct functype *ft, const struct cell *params,
                    struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        return args_environ_sizes_get(ctx, wasi, ft, params, results,
                                      wasi->argc, wasi->argv);
}

int
wasi_args_get(struct exec_context *ctx, struct host_instance *hi,
              const struct functype *ft, const struct cell *params,
              struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        return args_environ_get(ctx, wasi, ft, params, results, wasi->argc,
                                wasi->argv);
}

int
wasi_environ_sizes_get(struct exec_context *ctx, struct host_instance *hi,
                       const struct functype *ft, const struct cell *params,
                       struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        return args_environ_sizes_get(ctx, wasi, ft, params, results,
                                      wasi->nenvs, wasi->envs);
}

int
wasi_environ_get(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        return args_environ_get(ctx, wasi, ft, params, results, wasi->nenvs,
                                wasi->envs);
}
