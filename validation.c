#define _POSIX_C_SOURCE 199509L /* flockfile */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "type.h"
#include "util.h"
#include "xlog.h"

int
push_valtype(enum valtype type, struct validation_context *ctx)
{
        uint32_t nsize = ctx->nvaltypes + 1;
        int ret;

        assert(type != TYPE_ANYREF);
        if (nsize == 0) {
                return EOVERFLOW;
        }
        ret = resize_array((void **)&ctx->valtypes, sizeof(*ctx->valtypes),
                           nsize);
        if (ret != 0) {
                return ret;
        }
        ctx->valtypes[ctx->nvaltypes] = type;
        ctx->nvaltypes = nsize;
        if (ctx->nvaltypes > ctx->ei->maxvals) {
                ctx->ei->maxvals = ctx->nvaltypes;
        }
        return 0;
}

int
pop_valtype(enum valtype expected_type, enum valtype *typep,
            struct validation_context *ctx)
{
        const struct ctrlframe *cframe;

        assert(ctx->cframes != NULL);
        assert(ctx->ncframes > 0);
        cframe = &ctx->cframes[ctx->ncframes - 1];
        assert(ctx->nvaltypes >= cframe->height);
        if (ctx->nvaltypes == cframe->height) {
                if (cframe->unreachable) {
                        *typep = TYPE_UNKNOWN;
                        return 0;
                }
                return EINVAL;
        }
        ctx->nvaltypes--;
        enum valtype t = ctx->valtypes[ctx->nvaltypes];
        assert(t != TYPE_ANYREF);
        if (expected_type != TYPE_UNKNOWN && t != TYPE_UNKNOWN &&
            t != expected_type &&
            !(expected_type == TYPE_ANYREF && is_reftype(t))) {
                return validation_failure(ctx, "expected %x actual %x",
                                          expected_type, t);
        }
        *typep = t;
        return 0;
}

int
push_valtypes(const struct resulttype *types, struct validation_context *ctx)
{
        uint32_t i;

        for (i = 0; i < types->ntypes; i++) {
                int ret;

                ret = push_valtype(types->types[i], ctx);
                if (ret != 0) {
                        return ret;
                }
        }
        return 0;
}

int
pop_valtypes(const struct resulttype *types, struct validation_context *ctx)
{
        uint32_t i;

        if (types->ntypes == 0) {
                return 0;
        }

        for (i = types->ntypes - 1;;) {
                enum valtype t;
                int ret;

                ret = pop_valtype(types->types[i], &t, ctx);
                if (ret != 0) {
                        return ret;
                }
                if (i == 0) {
                        break;
                }
                i--;
        }
        return 0;
}

int
peek_valtypes(const struct resulttype *types, struct validation_context *ctx)
{
        uint32_t saved_height = ctx->nvaltypes;
        int ret = pop_valtypes(types, ctx);
        ctx->nvaltypes = saved_height;
        return ret;
}

int
push_ctrlframe(uint32_t pc, enum ctrlframe_op op, uint32_t jumpslot,
               struct resulttype *start_types, struct resulttype *end_types,
               struct validation_context *ctx)
{
        struct ctrlframe *cframe;
        int ret;

        xlog_trace("push_ctrlframe (op %02x) %u %u", (unsigned int)op,
                   start_types != NULL ? start_types->ntypes : 0,
                   end_types->ntypes);
        struct expr_exec_info *ei = ctx->ei;
        uint32_t nslots = 1;
        /*
         * for "else", restore the slot of "if".
         * it will be used by "end".
         */
        assert(op == FRAME_OP_ELSE || jumpslot == 0);
        if (!ctx->generate_jump_table || op == FRAME_OP_INVOKE ||
            op == FRAME_OP_ELSE) {
                nslots = 0;
        } else if (op == FRAME_OP_IF) {
                /*
                 * allocate two slots.
                 * the extra slot is for the "jump to else" case.
                 */
                nslots = 2;
        }
        if (nslots > 0) {
                ret = resize_array((void **)&ei->jumps, sizeof(*ei->jumps),
                                   ei->njumps + nslots);
                if (ret != 0) {
                        return ret;
                }
        }
        ret = resize_array((void **)&ctx->cframes, sizeof(*ctx->cframes),
                           ctx->ncframes + 1);
        if (ret != 0) {
                return ret;
        }
        if (nslots > 0) {
                jumpslot = ei->njumps;
                ei->njumps += nslots;
                ei->jumps[jumpslot].pc = pc;
                ei->jumps[jumpslot].targetpc = 0;
                if (nslots == 2) {
                        /*
                         * the slot for "if -> else".
                         * targetpc will be left as 0 if this block has
                         * no "else".
                         */
                        ei->jumps[jumpslot + 1].pc = pc + 1;
                        ei->jumps[jumpslot + 1].targetpc = 0;
                }
        }
        cframe = &ctx->cframes[ctx->ncframes];
        cframe->op = op;
        cframe->jumpslot = jumpslot;
        cframe->start_types = start_types;
        cframe->end_types = end_types;
        cframe->unreachable = false;
        cframe->height = ctx->nvaltypes;
        ctx->ncframes++;
        if (ctx->ncframes > ei->maxlabels) {
                ei->maxlabels = ctx->ncframes;
        }
        if (start_types == NULL) {
                return 0;
        }
        return push_valtypes(start_types, ctx);
}

int
pop_ctrlframe(uint32_t pc, bool is_else, struct ctrlframe *cframep,
              struct validation_context *ctx)
{
        struct ctrlframe *cframe;
        int ret;

        if (ctx->ncframes == 0) {
                return EINVAL;
        }
        cframe = &ctx->cframes[ctx->ncframes - 1];
        if (is_else && cframe->op != FRAME_OP_IF) {
                return EINVAL;
        }
        if (cframe->op != FRAME_OP_INVOKE && ctx->ei->jumps != NULL) {
                struct jump *jump =
                        &ctx->ei->jumps[cframe->jumpslot + is_else];
                assert(jump->pc != 0);
                assert(jump->targetpc == 0);
                if (cframe->op == FRAME_OP_LOOP) {
                        jump->targetpc = jump->pc;
                } else {
                        jump->targetpc = pc;
                }
        }
        ret = pop_valtypes(cframe->end_types, ctx);
        if (ret != 0) {
                return EINVAL;
        }
        if (ctx->nvaltypes != cframe->height) {
                return validation_failure(ctx,
                                          "unexpected stack height after "
                                          "popping cframe: %" PRIu32
                                          " != %" PRIu32,
                                          ctx->nvaltypes, cframe->height);
        }
        *cframep = *cframe;
        ctx->ncframes--;
        return 0;
}

void
mark_unreachable(struct validation_context *ctx)
{
        struct ctrlframe *cframe = &ctx->cframes[ctx->ncframes - 1];
        ctx->nvaltypes = cframe->height;
        cframe->unreachable = true;
}

const struct resulttype *
label_types(struct ctrlframe *cframe)
{
        if (cframe->op == FRAME_OP_LOOP) {
                return cframe->start_types;
        }
        return cframe->end_types;
}

int
validation_failure(struct validation_context *ctx, const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        vreport(ctx->report, fmt, ap);
        va_end(ap);
        return EINVAL;
}

struct resulttype *
returntype(struct validation_context *ctx)
{
        return ctx->cframes[0].end_types;
}

void
validation_context_init(struct validation_context *ctx)
{
        memset(ctx, 0, sizeof(*ctx));
}

void
validation_context_clear(struct validation_context *ctx)
{
        uint32_t i;

        for (i = 0; i < ctx->ncframes; i++) {
                ctrlframe_clear(&ctx->cframes[i]);
        }
        free(ctx->cframes);
        free(ctx->valtypes);
        free(ctx->locals);
}

void
ctrlframe_clear(struct ctrlframe *cframe)
{
        resulttype_free(cframe->end_types);
        resulttype_free(cframe->start_types);
}

int
target_label_types(struct validation_context *ctx, uint32_t labelidx,
                   const struct resulttype **rtp)
{
        if (labelidx > ctx->ncframes) {
                return EINVAL;
        }
        struct ctrlframe *cframe = &ctx->cframes[ctx->ncframes - labelidx - 1];
        const struct resulttype *rt = label_types(cframe);
        *rtp = rt;
        return 0;
}
