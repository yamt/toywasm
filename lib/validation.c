#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "options.h"
#include "report.h"
#include "type.h"
#include "util.h"
#include "validation.h"
#include "xlog.h"

static const struct ctrlframe *
current_frame(const struct validation_context *ctx)
{
        assert(ctx->cframes.lsize > 0);
        const struct ctrlframe *cframe = &VEC_LASTELEM(ctx->cframes);
        return cframe;
}

static bool
cframe_unreachable(const struct ctrlframe *cframe)
{
        return cframe->unreachable;
}

static void
push_valtype_common(enum valtype type, const struct ctrlframe *cframe,
                    struct validation_context *ctx)
{
        const bool unreachable = cframe_unreachable(cframe);
        assert(type != TYPE_ANYREF);
        /*
         * we sometimes push TYPE_UNKNOWN onto the stack.
         *
         * eg.
         *    block (result i32)
         *      i32.const 0
         *      br 0
         *      i32.const 0
         *      select ;; here
         *    end
         */
        assert(type != TYPE_UNKNOWN || unreachable);
        assert(ctx->valtypes.lsize < ctx->valtypes.psize);
        *VEC_PUSH(ctx->valtypes) = type;
        if (!unreachable) {
#if defined(TOYWASM_USE_SMALL_CELLS)
                ctx->ncells += valtype_cellsize(type);
                uint32_t ncells = ctx->ncells;
#else
                uint32_t ncells = ctx->valtypes.lsize;
#endif
                if (ncells > ctx->ei->maxcells) {
                        ctx->ei->maxcells = ncells;
                }
        }
}

int
push_valtype(enum valtype type, struct validation_context *ctx)
{
        int ret = VEC_PREALLOC(ctx->valtypes, 1);
        if (ret != 0) {
                return ret;
        }
        push_valtype_common(type, current_frame(ctx), ctx);
        return 0;
}

static int
pop_valtype_common(enum valtype expected_type, enum valtype *typep,
                   const struct ctrlframe *cframe,
                   struct validation_context *ctx)
{
        const bool unreachable = cframe_unreachable(cframe);
        assert(ctx->valtypes.lsize >= cframe->height);
#if defined(TOYWASM_USE_SMALL_CELLS)
        assert(ctx->ncells >= cframe->height_cell);
#endif
        if (ctx->valtypes.lsize == cframe->height) {
#if defined(TOYWASM_USE_SMALL_CELLS)
                assert(ctx->ncells == cframe->height_cell);
#endif
                if (unreachable) {
                        *typep = TYPE_UNKNOWN;
                        return 0;
                }
                return EINVAL;
        }
        enum valtype t = *VEC_POP(ctx->valtypes);
        assert(t != TYPE_ANYREF);
        assert(t != TYPE_UNKNOWN || unreachable);
#if defined(TOYWASM_USE_SMALL_CELLS)
        if (!unreachable) {
                uint32_t csz = valtype_cellsize(t);
                assert(ctx->ncells >= csz);
                ctx->ncells -= csz;
        }
#endif
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
pop_valtype(enum valtype expected_type, enum valtype *typep,
            struct validation_context *ctx)
{
        return pop_valtype_common(expected_type, typep, current_frame(ctx),
                                  ctx);
}

int
push_valtypes(const struct resulttype *types, struct validation_context *ctx)
{
        int ret;
        ret = VEC_PREALLOC(ctx->valtypes, types->ntypes);
        if (ret != 0) {
                return ret;
        }
        const struct ctrlframe *cframe = current_frame(ctx);
        uint32_t i;
        for (i = 0; i < types->ntypes; i++) {
                push_valtype_common(types->types[i], cframe, ctx);
        }
        return 0;
}

int
pop_valtypes(const struct resulttype *types, struct validation_context *ctx)
{
        const struct ctrlframe *cframe = current_frame(ctx);
        uint32_t left = types->ntypes;
        while (left > 0) {
                left--;
                enum valtype t;
                int ret = pop_valtype_common(types->types[left], &t, cframe,
                                             ctx);
                if (ret != 0) {
                        return ret;
                }
        }
        return 0;
}

int
peek_valtypes(const struct resulttype *types, struct validation_context *ctx)
{
        uint32_t saved_height = ctx->valtypes.lsize;
#if defined(TOYWASM_USE_SMALL_CELLS)
        uint32_t saved_ncells = ctx->ncells;
#endif
        int ret = pop_valtypes(types, ctx);
        ctx->valtypes.lsize = saved_height;
#if defined(TOYWASM_USE_SMALL_CELLS)
        ctx->ncells = saved_ncells;
#endif
        return ret;
}

int
push_ctrlframe(uint32_t pc, enum ctrlframe_op op, uint32_t jumpslot,
               struct resulttype *start_types, struct resulttype *end_types,
               struct validation_context *ctx)
{
        struct ctrlframe *cframe;
        int ret;

        xlog_trace_insn("push_ctrlframe (op %02x) start_types.size %" PRIu32
                        " end_types.size %" PRIu32 " vals.size %" PRIu32,
                        (unsigned int)op,
                        start_types != NULL ? start_types->ntypes : 0,
                        end_types->ntypes, ctx->valtypes.lsize);
        struct expr_exec_info *ei = ctx->ei;
        uint32_t nslots = 1;
        /*
         * for "else", restore the slot of "if".
         * it will be used by "end".
         */
        assert(op == FRAME_OP_ELSE || jumpslot == 0);
        if (!ctx->options->generate_jump_table || op == FRAME_OP_INVOKE ||
            op == FRAME_OP_ELSE || op == FRAME_OP_EMPTY_ELSE ||
            op == FRAME_OP_LOOP) {
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
        ret = VEC_PREALLOC(ctx->cframes, 1);
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
        cframe = VEC_PUSH(ctx->cframes);
        cframe->op = op;
        cframe->jumpslot = jumpslot;
        cframe->start_types = start_types;
        cframe->end_types = end_types;
        cframe->unreachable = false;
        cframe->height = ctx->valtypes.lsize;
#if defined(TOYWASM_USE_SMALL_CELLS)
        cframe->height_cell = ctx->ncells;
#endif
        if (ctx->cframes.lsize > ei->maxlabels) {
                ei->maxlabels = ctx->cframes.lsize;
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

        if (ctx->cframes.lsize == 0) {
                return EINVAL;
        }
        cframe = &VEC_LASTELEM(ctx->cframes);
        if (is_else && cframe->op != FRAME_OP_IF) {
                return validation_failure(ctx, "if-else mismatch");
        }
        if (cframe->op != FRAME_OP_INVOKE &&
            cframe->op != FRAME_OP_EMPTY_ELSE && cframe->op != FRAME_OP_LOOP &&
            ctx->ei->jumps != NULL) {
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
        if (ctx->valtypes.lsize != cframe->height) {
                return validation_failure(ctx,
                                          "unexpected stack height after "
                                          "popping cframe: %" PRIu32
                                          " != %" PRIu32,
                                          ctx->valtypes.lsize, cframe->height);
        }
#if defined(TOYWASM_USE_SMALL_CELLS)
        assert(ctx->ncells == cframe->height_cell);
#endif
        *cframep = *cframe;
        ctx->cframes.lsize--;
        return 0;
}

void
mark_unreachable(struct validation_context *ctx)
{
        struct ctrlframe *cframe = &VEC_LASTELEM(ctx->cframes);
        ctx->valtypes.lsize = cframe->height;
#if defined(TOYWASM_USE_SMALL_CELLS)
        ctx->ncells = cframe->height_cell;
#endif
        cframe->unreachable = true;
}

const struct resulttype *
label_types(const struct ctrlframe *cframe)
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
        return VEC_ELEM(ctx->cframes, 0).end_types;
}

void
validation_context_init(struct validation_context *ctx)
{
        memset(ctx, 0, sizeof(*ctx));
}

void
validation_context_reuse(struct validation_context *ctx)
{
        struct ctrlframe *cframe;
        VEC_FOREACH(cframe, ctx->cframes) {
                ctrlframe_clear(cframe);
        }
        ctx->cframes.lsize = 0;
        ctx->valtypes.lsize = 0;
#if defined(TOYWASM_USE_SMALL_CELLS)
        ctx->ncells = 0;
#endif
        ctx->locals.lsize = 0;
}

void
validation_context_clear(struct validation_context *ctx)
{
        validation_context_reuse(ctx);
        VEC_FREE(ctx->cframes);
        VEC_FREE(ctx->valtypes);
        VEC_FREE(ctx->locals);
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
        if (labelidx >= ctx->cframes.lsize) {
                return EINVAL;
        }
        const struct ctrlframe *cframe =
                &VEC_ELEM(ctx->cframes, ctx->cframes.lsize - labelidx - 1);
        const struct resulttype *rt = label_types(cframe);
        *rtp = rt;
        return 0;
}

int
record_type_annotation(struct validation_context *vctx, const uint8_t *p,
                       enum valtype t)
{
#if defined(TOYWASM_USE_SMALL_CELLS)
        const struct ctrlframe *cframe = &VEC_LASTELEM(vctx->cframes);
        if (cframe->unreachable) {
                assert(is_valtype(t) || t == TYPE_UNKNOWN);
                return 0;
        }
        assert(is_valtype(t));
        const uint32_t csz = valtype_cellsize(t);
        struct expr_exec_info *ei = vctx->ei;
        struct type_annotations *an = &ei->type_annotations;
        if (an->default_size == 0) {
                an->default_size = csz;
                return 0;
        }
        const uint32_t pc = ptr2pc(vctx->module, p);
        if (an->ntypes == 0) {
                if (an->default_size == csz) {
                        return 0;
                }
        } else {
                assert(an->types[an->ntypes - 1].pc < pc);
                if (an->types[an->ntypes - 1].size == t) {
                        return 0;
                }
        }
        int ret;
        ret = resize_array((void **)&an->types, sizeof(*an->types),
                           an->ntypes + 1);
        if (ret != 0) {
                return ret;
        }
        an->types[an->ntypes].pc = pc;
        an->types[an->ntypes].size = csz;
        an->ntypes++;
#endif
        return 0;
}
