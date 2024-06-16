#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "load_context.h"
#include "validation.h"

void
load_context_init(struct load_context *ctx, struct mem_context *mctx)
{
        memset(ctx, 0, sizeof(*ctx));
        ctx->mctx = mctx;
        report_init(&ctx->report);
        load_options_set_defaults(&ctx->options);
}

void
load_context_clear(struct load_context *ctx)
{
        struct mem_context *mctx = load_mctx(ctx);
        report_clear(&ctx->report);
        bitmap_free(mctx, &ctx->refs, ctx->refs_size);
        if (ctx->vctx != NULL) {
                validation_context_clear(ctx->vctx);
                free(ctx->vctx);
        }
}
