#include "restart.h"
#include "exec_context.h"
#include "vec.h"

int
restart_info_prealloc(struct exec_context *ctx)
{
        if (ctx->restarts.lsize < ctx->restarts.psize) {
                return 0;
        }
        int ret = VEC_PREALLOC(ctx->restarts, 1);
        if (ret != 0) {
                return ret;
        }
        VEC_NEXTELEM(ctx->restarts).restart_type = RESTART_NONE;
        return 0;
}

void
restart_info_clear(struct exec_context *ctx)
{
        if (ctx->restarts.lsize < ctx->restarts.psize) {
                struct restart_info *restart = &VEC_NEXTELEM(ctx->restarts);
                restart->restart_type = RESTART_NONE;
        }
}

bool
restart_info_is_none(struct exec_context *ctx)
{
        return ctx->restarts.lsize == ctx->restarts.psize ||
               VEC_NEXTELEM(ctx->restarts).restart_type == RESTART_NONE;
}
