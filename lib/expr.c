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
#include "validation.h"
#include "xlog.h"

static int
read_op(const uint8_t **pp, const uint8_t *ep,
        const struct instruction_desc **descp)
{
        const struct instruction_desc *table = instructions;
        size_t table_size = instructions_size;
        const char *group = "base";
        int ret;
        uint8_t inst8;
        uint32_t inst;

        ret = read_u8(pp, ep, &inst8);
        if (ret != 0) {
                goto fail;
        }
        inst = inst8;
        while (true) {
                const struct instruction_desc *desc;
                if (inst >= table_size) {
                        goto invalid_inst;
                }
                desc = &table[inst];
                if (desc->next_table != NULL) {
                        table = desc->next_table;
                        table_size = desc->next_table_size;
                        group = desc->name;
                        /*
                         * Note: wasm "sub" opcodes are LEB128.
                         * cf. https://github.com/WebAssembly/spec/issues/1228
                         */
                        ret = read_leb_u32(pp, ep, &inst);
                        if (ret != 0) {
                                goto fail;
                        }
                        continue;
                }
                if (desc->name == NULL) {
invalid_inst:
                        xlog_error("unimplemented instruction %02" PRIx32
                                   " in group '%s'",
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

static int
read_expr_common(const uint8_t **pp, const uint8_t *ep, struct expr *expr,
                 uint32_t nlocals, const struct localchunk *locals,
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
        vctx->report = &lctx->report;
        vctx->refs = &lctx->refs;
        vctx->const_expr = const_expr;
        vctx->module = lctx->module;
        vctx->has_datacount = lctx->has_datacount;
        vctx->ndatas_in_datacount = lctx->ndatas_in_datacount;
        vctx->options = &lctx->options;
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
        const struct localchunk *ch = locals;
        for (i = 0; i < nlocals; i += ch->n, ch++) {
                uint32_t j;
                for (j = 0; j < ch->n; j++) {
                        vctx->locals[parameter_types->ntypes + i + j] =
                                ch->type;
                }
        }

        expr->start = p;

        /* push the implicit frame */
        ret = push_ctrlframe(0, FRAME_OP_INVOKE, 0, NULL, result_types, vctx);
        if (ret != 0) {
                goto fail;
        }
        while (true) {
                const struct instruction_desc *desc;

#if defined(TOYWASM_ENABLE_TRACING_INSN)
                uint32_t pc = ptr2pc(vctx->module, p);
#endif
                ret = read_op(&p, ep, &desc);
                if (ret != 0) {
                        goto fail;
                }
                xlog_trace_insn("inst %06" PRIx32 " %s", pc, desc->name);
#if defined(TOYWASM_ENABLE_TRACING_INSN)
                uint32_t orig_n = vctx->valtypes.lsize;
#endif
                if (const_expr && (desc->flags & INSN_FLAG_CONST) == 0) {
                        ret = validation_failure(vctx,
                                                 "instruction \"%s\" not "
                                                 "allowed in a const expr",
                                                 desc->name);
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
                xlog_trace_insn("inst %s %u %u: %" PRIu32 " -> %" PRIu32,
                                desc->name, vctx->ncframes,
                                vctx->cframes[vctx->ncframes - 1].height,
                                orig_n, vctx->valtypes.lsize);
        }
#if defined(TOYWASM_ENABLE_TRACING_INSN)
        for (i = 0; i < ei->njumps; i++) {
                const struct jump *j = &ei->jumps[i];
                xlog_trace_insn("jump table [%" PRIu32 "] %06" PRIx32
                                " -> %06" PRIx32,
                                i, j->pc, j->targetpc);
        }
#endif
        *pp = p;
#if defined(TOYWASM_ENABLE_WRITER)
        expr->end = p;
#endif
        xlog_trace("code size %zu, jump table size %zu, max labels %" PRIu32
                   ", cells %" PRIu32,
                   expr->end - expr->start, ei->njumps * sizeof(*ei->jumps),
                   ei->maxlabels, ei->maxcells);
        validation_context_clear(vctx);
        return 0;
fail:
        free(ei->jumps);
        validation_context_clear(vctx);
        return ret;
}

int
read_expr(const uint8_t **pp, const uint8_t *ep, struct expr *expr,
          uint32_t nlocals, const struct localchunk *chunks,
          struct resulttype *parameter_types, struct resulttype *result_types,
          struct load_context *lctx)
{
        return read_expr_common(pp, ep, expr, nlocals, chunks, parameter_types,
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
#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
                .cellidx =
                        {
                                NULL,
                        },
#endif
        };
        struct resulttype resulttype = {
                .ntypes = 1,
                .types = &type,
                .is_static = true,
#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
                .cellidx =
                        {
                                NULL,
                        },
#endif
        };

        return read_expr_common(pp, ep, expr, 0, NULL, &empty, &resulttype,
                                true, lctx);
}
