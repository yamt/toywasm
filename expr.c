#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "decode.h"
#include "endian.h"
#include "exec.h"
#include "expr.h"
#include "insn.h"
#include "leb128.h"
#include "load_context.h"
#include "type.h"
#include "util.h"
#include "xlog.h"

int
read_op(const uint8_t **pp, const uint8_t *ep,
        const struct instruction_desc **descp)
{
        const struct instruction_desc *table = instructions;
        size_t table_size = instructions_size;
        const char *group = "base";
        int ret;

        while (true) {
                const struct instruction_desc *desc;
                uint8_t inst;
                ret = read_u8(pp, ep, &inst);
                if (ret != 0) {
                        goto fail;
                }
                if (inst >= table_size) {
                        goto invalid_inst;
                }
                desc = &table[inst];
                if (desc->next_table != NULL) {
                        table = desc->next_table;
                        table_size = desc->next_table_size;
                        group = desc->name;
                        continue;
                }
                if (desc->name == NULL) {
invalid_inst:
                        xlog_error(
                                "unimplemented instruction %02x in group '%s'",
                                inst, group);
                        ret = EINVAL;
                        goto fail;
                }
                *descp = desc;
                break;
        }
        ret = 0;
fail:
        return ret;
}

int
read_expr_common(const uint8_t **pp, const uint8_t *ep, struct expr *expr,
                 uint32_t nlocals, const enum valtype *locals,
                 struct resulttype *parameter_types,
                 struct resulttype *result_types, bool const_expr,
                 struct load_context *lctx)
{
        const uint8_t *p = *pp;
        struct context ctx0;
        struct context *ctx = &ctx0;
        struct validation_context vctx0;
        struct validation_context *vctx = &vctx0;
        int ret;

        memset(ctx, 0, sizeof(*ctx));
        ctx->validation = vctx;

        assert(lctx->module != NULL);
        validation_context_init(vctx);
        vctx->const_expr = const_expr;
        vctx->generate_jump_table = lctx->generate_jump_table;
        vctx->module = lctx->module;
        struct expr_exec_info *ei;
        vctx->ei = ei = &expr->ei;
        memset(ei, 0, sizeof(*ei));

        vctx->nlocals = parameter_types->ntypes + nlocals;
        ret = ARRAY_RESIZE(vctx->locals, vctx->nlocals);
        if (ret != 0) {
                goto fail;
        }
        uint32_t i;
        for (i = 0; i < parameter_types->ntypes; i++) {
                vctx->locals[i] = parameter_types->types[i];
        }
        for (i = 0; i < nlocals; i++) {
                vctx->locals[parameter_types->ntypes + i] = locals[i];
        }

        expr->start = p;

        /* push the implicit frame */
        ret = push_ctrlframe(0, FRAME_OP_INVOKE, 0, NULL, result_types, vctx);
        if (ret != 0) {
                goto fail;
        }
        while (true) {
                const struct instruction_desc *desc;

#if defined(ENABLE_TRACING)
                uint32_t pc = ptr2pc(vctx->module, p);
#endif
                ret = read_op(&p, ep, &desc);
                if (ret != 0) {
                        goto fail;
                }
                xlog_trace("inst %06" PRIx32 " %s", pc, desc->name);
#if defined(ENABLE_TRACING)
                uint32_t orig_n = vctx->nvaltypes;
#endif
                if (const_expr && (desc->flags & INSN_FLAG_CONST) == 0) {
                        xlog_trace("instruction not allowed in a const expr");
                        ret = EINVAL;
                        goto fail;
                }
                if (desc->process != NULL) {
                        ret = desc->process(&p, ep, ctx);
                        if (ret != 0) {
                                goto fail;
                        }
                }
                if (vctx->ncframes == 0) {
                        break;
                }
                xlog_trace("inst %s %u %u: %u -> %u", desc->name,
                           vctx->ncframes,
                           vctx->cframes[vctx->ncframes - 1].height, orig_n,
                           vctx->nvaltypes);
        }
#if defined(ENABLE_TRACING)
        for (i = 0; i < ei->njumps; i++) {
                const struct jump *j = &ei->jumps[i];
                xlog_trace("jump table [%" PRIu32 "] %06" PRIx32
                           " -> %06" PRIx32,
                           i, j->pc, j->targetpc);
        }
#endif
        *pp = p;
        expr->end = p;
        xlog_trace("code size %zu, jump table size %zu, max labels %" PRIu32
                   ", vals %" PRIu32,
                   expr->end - expr->start, ei->njumps * sizeof(*ei->jumps),
                   ei->maxlabels, ei->maxvals);
        validation_context_clear(vctx);
        return 0;
fail:
        free(ei->jumps);
        validation_context_clear(vctx);
        return ret;
}

int
read_expr(const uint8_t **pp, const uint8_t *ep, struct expr *expr,
          uint32_t nlocals, const enum valtype *locals,
          struct resulttype *parameter_types, struct resulttype *result_types,
          struct load_context *lctx)
{
        return read_expr_common(pp, ep, expr, nlocals, locals, parameter_types,
                                result_types, false, lctx);
}

/*
 * https://webassembly.github.io/spec/core/valid/instructions.html#constant-expressions
 */
int
read_const_expr(const uint8_t **pp, const uint8_t *ep, struct expr *expr,
                enum valtype type, struct load_context *lctx)
{
        static struct resulttype empty = {
                .ntypes = 0,
                .is_static = true,
        };
        struct resulttype resulttype = {
                .ntypes = 1,
                .types = &type,
                .is_static = true,
        };

        return read_expr_common(pp, ep, expr, 0, NULL, &empty, &resulttype,
                                true, lctx);
}
