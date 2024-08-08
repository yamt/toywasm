#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "endian.h"
#include "wasi_host_subr.h"
#include "wasi_impl.h"
#include "xlog.h"

#include "wasi_hostfuncs.h"

int
wasi_clock_res_get(struct exec_context *ctx, struct host_instance *hi,
                   const struct functype *ft, const struct cell *params,
                   struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t clockid = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 1, i32);
        clockid_t hostclockid;
        int host_ret = 0;
        int ret;
        ret = wasi_convert_clockid(clockid, &hostclockid);
        if (ret != 0) {
                goto fail;
        }
        struct timespec ts;
        ret = clock_getres(hostclockid, &ts);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        uint64_t result = host_to_le64(timespec_to_ns(&ts));
        host_ret = wasi_copyout(ctx, wasi_memory(wasi), &result, retp,
                                sizeof(result), WASI_U64_ALIGN);
fail:
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

int
wasi_clock_time_get(struct exec_context *ctx, struct host_instance *hi,
                    const struct functype *ft, const struct cell *params,
                    struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t clockid = HOST_FUNC_PARAM(ft, params, 0, i32);
#if 0 /* REVISIT what to do with the precision? */
        uint64_t precision = HOST_FUNC_PARAM(ft, params, 1, i64);
#endif
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 2, i32);
        clockid_t hostclockid;
        int host_ret = 0;
        int ret;
        ret = wasi_convert_clockid(clockid, &hostclockid);
        if (ret != 0) {
                goto fail;
        }
        struct timespec ts;
        ret = clock_gettime(hostclockid, &ts);
        if (ret == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        uint64_t result = host_to_le64(timespec_to_ns(&ts));
        host_ret = wasi_copyout(ctx, wasi_memory(wasi), &result, retp,
                                sizeof(result), WASI_U64_ALIGN);
fail:
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}
