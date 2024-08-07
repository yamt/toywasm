#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#define _NETBSD_SOURCE

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GLIBC__) || defined(__NuttX__)
#include <sys/random.h> /* getrandom */
#endif

#include "endian.h"
#include "wasi_impl.h"
#include "xlog.h"

#include "wasi_hostfuncs.h"

int
wasi_random_get(struct exec_context *ctx, struct host_instance *hi,
                const struct functype *ft, const struct cell *params,
                struct cell *results)
{
        WASI_TRACE;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t buf = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t buflen = HOST_FUNC_PARAM(ft, params, 1, i32);
        int ret = 0;
        void *p;
        int host_ret = host_func_memory_getptr(ctx, 0, buf, 0, buflen, &p);
        if (host_ret != 0) {
                goto fail;
        }
#if defined(__GLIBC__) || defined(__NuttX__)
        /*
         * glibc doesn't have arc4random
         * https://sourceware.org/bugzilla/show_bug.cgi?id=4417
         *
         * NuttX has both of getrandom and arc4random_buf.
         * The latter is available only if CONFIG_CRYPTO_RANDOM_POOL=y.
         */
        while (buflen > 0) {
                ssize_t ssz = getrandom(p, buflen, 0);
                if (ssz == -1) {
                        ret = errno;
                        assert(ret > 0);
                        if (ret == EINTR) {
                                continue;
                        }
                        break;
                }
                p = (uint8_t *)p + ssz;
                buflen -= ssz;
        }
#else
        arc4random_buf(p, buflen);
        ret = 0;
#endif
fail:
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32,
                                     wasi_convert_errno(ret));
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}
