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
#include "expr_parser.h"
#include "insn.h"
#include "leb128.h"
#include "load_context.h"
#include "mem.h"
#include "type.h"
#include "util.h"
#include "validation.h"
#include "xlog.h"

static int
read_op(const uint8_t **pp, const uint8_t *ep,
        const struct instruction_desc **descp, struct validation_context *vctx)
{
        const struct instruction_desc *table = instructions;
        size_t table_size = instructions_size;
        const char *group = "base";
        int ret;
        uint8_t inst8;
        uint32_t inst;

#if defined(TOYWASM_ENABLE_TRACING_INSN)
        uint32_t pc = ptr2pc(vctx->module, *pp);
#endif
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
                        ret = validation_failure(
                                vctx,
                                "unimplemented instruction %02" PRIx32
                                " in group '%s'",
                                inst, group);
                        goto fail;
                }
                *descp = desc;
                xlog_trace_insn("inst %06" PRIx32 " %s", pc, desc->name);
                break;
        }
        ret = 0;
fail:
        return ret;
}

static int
check_const_instruction(const struct instruction_desc *desc,
                        struct validation_context *vctx)
{
        if (vctx->const_expr && (desc->flags & INSN_FLAG_CONST) == 0) {
                return validation_failure(vctx,
                                          "instruction \"%s\" not "
                                          "allowed in a const expr",
                                          desc->name);
        }
        return 0;
}

#if defined(TOYWASM_USE_SEPARATE_VALIDATE)
int
fetch_validate_next_insn(const uint8_t *p, const uint8_t *ep,
                         struct validation_context *vctx)
{
        xassert(ep != NULL);
        const struct instruction_desc *desc;
        int ret;

        ret = read_op(&p, ep, &desc, vctx);
        if (ret != 0) {
                goto fail;
        }
        ret = check_const_instruction(desc, vctx);
        if (ret != 0) {
                goto fail;
        }
#if defined(TOYWASM_USE_TAILCALL)
        __musttail
#endif
                return desc->validate(p, ep, vctx);
fail:
        return ret;
}
#else
int
fetch_process_next_insn(const uint8_t **pp, const uint8_t *ep,
                        struct context *ctx)
{
        xassert(ep != NULL);
        struct validation_context *vctx = ctx->validation;

        const struct instruction_desc *desc;
        int ret;

        ret = read_op(pp, ep, &desc, vctx);
        if (ret != 0) {
                goto fail;
        }
        ret = check_const_instruction(desc, vctx);
        if (ret != 0) {
                goto fail;
        }
        return desc->process(pp, ep, ctx);
fail:
        return ret;
}
#endif /* defined(TOYWASM_USE_SEPARATE_VALIDATE) */

static int
read_expr_common(const uint8_t **pp, const uint8_t *ep, struct expr *expr,
                 uint32_t nlocals, const struct localchunk *locals,
                 struct resulttype *parameter_types,
                 struct resulttype *result_types, bool const_expr,
                 struct load_context *lctx)
{
        xassert(ep != NULL);
        const uint8_t *p = *pp;
        int ret;

        assert(lctx->module != NULL);
        struct mem_context *mctx = load_mctx(lctx);
        struct validation_context *vctx = lctx->vctx;
        if (vctx == NULL) {
                vctx = mem_alloc(mctx, sizeof(*vctx));
                if (vctx == NULL) {
                        return ENOMEM;
                }
                validation_context_init(vctx);
                lctx->vctx = vctx;
                vctx->mctx = mctx;

                vctx->module = lctx->module;
                vctx->report = &lctx->report;
                vctx->refs = &lctx->refs;
                vctx->options = &lctx->options;
        } else {
                assert(vctx->module == lctx->module);
                assert(vctx->report == &lctx->report);
                assert(vctx->refs == &lctx->refs);
                assert(vctx->options == &lctx->options);
        }
        /* note: const exprs can come before the datacount section */
        vctx->has_datacount = lctx->has_datacount;
        vctx->ndatas_in_datacount = lctx->ndatas_in_datacount;
        vctx->const_expr = const_expr;
        struct expr_exec_info *ei;
        vctx->ei = ei = &expr->ei;
        memset(ei, 0, sizeof(*ei));

        uint32_t lsize = parameter_types->ntypes + nlocals;
        ret = VEC_PREALLOC(vctx->mctx, vctx->locals, lsize);
        if (ret != 0) {
                goto fail;
        }
        uint32_t i;
        for (i = 0; i < parameter_types->ntypes; i++) {
                VEC_ELEM(vctx->locals, i) = parameter_types->types[i];
        }
        const struct localchunk *ch = locals;
        for (i = 0; i < nlocals; i += ch->n, ch++) {
                uint32_t j;
                for (j = 0; j < ch->n; j++) {
                        VEC_ELEM(vctx->locals,
                                 parameter_types->ntypes + i + j) = ch->type;
                }
        }
        vctx->locals.lsize = lsize;

        expr->start = p;

        /* push the implicit frame */
        ret = push_ctrlframe(0, FRAME_OP_INVOKE, 0, NULL, result_types, vctx);
        if (ret != 0) {
                goto fail;
        }

#if !defined(TOYWASM_USE_SEPARATE_VALIDATE)
        struct context ctx0;
        struct context *ctx = &ctx0;
        ctx->validation = vctx;
        ctx->exec = NULL;
#endif
        while (true) {
#if defined(TOYWASM_USE_SEPARATE_VALIDATE)
                ret = fetch_validate_next_insn(p, ep, vctx);
                if (ret != 0) {
                        goto fail;
                }
                assert(vctx->p > p);
                p = vctx->p;
#else
                ret = fetch_process_next_insn(&p, ep, ctx);
                if (ret != 0) {
                        goto fail;
                }
#endif
                assert(p <= ep);
                if (vctx->cframes.lsize == 0) {
                        break;
                }
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
#if defined(TOYWASM_MAINTAIN_EXPR_END)
        expr->end = p;
#endif
        xlog_trace("code size %zu, jump table size %zu, max labels %" PRIu32
                   ", cells %" PRIu32,
                   p - expr->start, ei->njumps * sizeof(*ei->jumps),
                   ei->maxlabels, ei->maxcells);
        validation_context_reuse(vctx);
        return 0;
fail:
        validation_context_reuse(vctx);
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
        DEFINE_RESULTTYPE(, resulttype, &type, 1);
        int ret = read_expr_common(pp, ep, expr, 0, NULL, empty_rt,
                                   &resulttype, true, lctx);
        /* a const expr does never require these annotations */
        assert(expr->ei.jumps == NULL);
#if defined(TOYWASM_USE_SMALL_CELLS)
        assert(expr->ei.type_annotations.types == NULL);
#endif
        return ret;
}

const uint8_t *
expr_end(const struct expr *expr)
{
#if defined(TOYWASM_MAINTAIN_EXPR_END)
        return expr->end;
#else
        struct parse_expr_context pctx;
        parse_expr_context_init(&pctx);
        const uint8_t *p = expr->start;
        const uint8_t *p1;
        do {
                p1 = p;
                parse_expr(&p, &pctx);
        } while (p != NULL);
        parse_expr_context_clear(&pctx);
        assert(*p1 == FRAME_OP_END);
        return p1 + 1; /* +1 for the end instruction */
#endif
}
