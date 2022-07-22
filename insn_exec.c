#include <assert.h>
#include <errno.h>
#include <inttypes.h>

#include "context.h"
#include "type.h"
#include "util.h"
#include "xlog.h"

int
push_val(const struct val *val, struct exec_context *ctx)
{
        *VEC_PUSH(ctx->stack) = *val;
        xlog_trace("stack push %016" PRIx64, val->u.i64);
        return 0;
}

int
pop_val(struct val *val, struct exec_context *ctx)
{
        assert(ctx->stack.lsize > 0);
        *val = *VEC_POP(ctx->stack);
        xlog_trace("stack pop  %016" PRIx64, val->u.i64);
        return 0;
}

void
push_label(struct exec_context *ctx)
{
        const uint8_t *p = ctx->p - 1;
        uint32_t pc = ptr2pc(ctx->instance->module, p);
        struct label *l = VEC_PUSH(ctx->labels);
        l->pc = pc;
        l->height = ctx->stack.lsize;
}

void
pop_label(struct exec_context *ctx)
{
        VEC_POP_DROP(ctx->labels);
}
