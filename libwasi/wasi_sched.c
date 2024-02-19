#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "endian.h"
#include "wasi_impl.h"
#include "xlog.h"

#include "wasi_hostfuncs.h"

int
wasi_sched_yield(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        int ret = 0;
        /* no-op */
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, wasi_convert_errno(ret));
        return 0;
}
