#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

#include "cluster.h"
#include "context.h"
#include "exec.h"
#include "expr.h"
#include "insn.h"
#include "leb128.h"
#include "platform.h"
#include "restart.h"
#include "suspend.h"
#include "timeutil.h"
#include "type.h"
#include "usched.h"
#include "util.h"
#include "xlog.h"

/*
 * Note: The C standard allows _Atomic types to have a different
 * size/alignment from their base types:
 *
 * > The size, representation, and alignment of an atomic type need not be
 * > the same as those of the corresponding unqualified type.
 *
 * Our atomic opcode implementation in insn_impl_threads.h have stronger
 * assumptions. We try to assert them below.
 *
 * Note: on WASM, atomic opcodes trap when the address is not a multiple
 * of the size of value.
 *
 * Note: on i386, alignof(uint64_t) == 4.
 */
_Static_assert(sizeof(_Atomic uint8_t) == sizeof(uint8_t), "atomic 8 size");
_Static_assert(sizeof(_Atomic uint16_t) == sizeof(uint16_t), "atomic 16 size");
_Static_assert(sizeof(_Atomic uint32_t) == sizeof(uint32_t), "atomic 32 size");
_Static_assert(sizeof(_Atomic uint64_t) == sizeof(uint64_t), "atomic 64 size");
_Static_assert(alignof(_Atomic uint8_t) <= 1, "atomic 8 align");
_Static_assert(alignof(_Atomic uint16_t) <= 2, "atomic 16 align");
_Static_assert(alignof(_Atomic uint32_t) <= 4, "atomic 32 align");
_Static_assert(alignof(_Atomic uint64_t) <= 8, "atomic 64 align");

void
frame_clear(struct funcframe *frame)
{
}

struct cell *
frame_locals(const struct exec_context *ctx, const struct funcframe *frame)
{
#if defined(TOYWASM_USE_SEPARATE_LOCALS)
        return &VEC_ELEM(ctx->locals, frame->localidx);
#else
        return &VEC_ELEM(ctx->stack, frame->height);
#endif
}

int
stack_prealloc(struct exec_context *ctx, uint32_t count)
{
        uint32_t needed = ctx->stack.lsize + count;
        if (needed > ctx->options.max_stackcells) {
                return trap_with_id(ctx, TRAP_TOO_MANY_STACKCELLS,
                                    "too many values on the operand stack");
        }
        return VEC_PREALLOC(ctx->stack, count);
}

static void
set_current_frame(struct exec_context *ctx, const struct funcframe *frame,
                  const struct expr_exec_info *ei)
{
        struct instance *inst = frame->instance;
        uint32_t funcidx = frame->funcidx;
        if (__predict_false(ctx->instance->module != inst->module)) {
                /*
                 * Invalidate jump cache.
                 *
                 * Note: because jump cache entires are currently
                 * keyed by PC, they are not safe to use among modules.
                 */
#if defined(TOYWASM_USE_JUMP_CACHE)
                ctx->jump_cache = NULL;
#endif
#if TOYWASM_JUMP_CACHE2_SIZE > 0
                memset(&ctx->cache, 0, sizeof(ctx->cache));
#endif
        }
        ctx->instance = inst;
        if (__predict_false(funcidx == FUNCIDX_INVALID)) {
                /*
                 * Exprs which is not in a function.
                 *
                 * Note: such exprs do never have function calls.
                 * (thus ei != NULL)
                 *
                 * Note: such exprs do never have parameters or locals.
                 * (thus no need to set ctx->fast or ctx->local_u)
                 */
                assert(ei != NULL);
                ctx->ei = ei;
        } else {
                const struct module *m = inst->module;
                const struct functype *ft = module_functype(m, funcidx);
                const struct func *func =
                        &m->funcs[funcidx - m->nimportedfuncs];
                assert(frame->nresults == resulttype_cellsize(&ft->result));
                assert(ei == NULL || ei == &func->e.ei);
#if defined(TOYWASM_USE_LOCALS_FAST_PATH)
                const uint16_t *paramtype_cellidxes =
                        ft->parameter.cellidx.cellidxes;
                const uint16_t *localtype_cellidxes =
                        func->localtype.cellidx.cellidxes;
                /*
                 * if we have both of indexes, use the fast path.
                 */
                ctx->fast = paramtype_cellidxes != NULL &&
                            localtype_cellidxes != NULL;
                if (ctx->fast) {
                        struct local_info_fast *fast = &ctx->local_u.fast;
                        fast->nparams = ft->parameter.ntypes;
                        fast->paramcsz = resulttype_cellsize(&ft->parameter);
                        fast->paramtype_cellidxes = paramtype_cellidxes;
                        fast->localtype_cellidxes = localtype_cellidxes;
                } else {
#endif
                        struct local_info_slow *slow = &ctx->local_u.slow;
                        slow->paramtype = &ft->parameter;
                        slow->localtype = &func->localtype;
#if defined(TOYWASM_USE_LOCALS_FAST_PATH)
                }
#endif
                ctx->ei = &func->e.ei;
        }
#if defined(TOYWASM_USE_LOCALS_CACHE)
        ctx->current_locals = frame_locals(ctx, frame);
#endif
}

static bool branch_to_label(struct exec_context *ctx, uint32_t labelidx,
                            bool goto_else, uint32_t *heightp,
                            uint32_t *arityp);

/*
 * https://webassembly.github.io/spec/core/exec/instructions.html#function-calls
 * https://webassembly.github.io/spec/core/exec/runtime.html#default-val
 */

/*
 * Note: the parameters of this function is redundant.
 * - for functions, localtype, paramtype, nresults can be obtained
 *   from instance+funcidx.
 * - const exprs, where funcidx is FUNCIDX_INVALID, do never have
 *   locals or parameters.
 */
int
frame_enter(struct exec_context *ctx, struct instance *inst, uint32_t funcidx,
            const struct expr_exec_info *ei, const struct localtype *localtype,
            const struct resulttype *paramtype, uint32_t nresults,
            const struct cell *params)
{
        assert(funcidx != FUNCIDX_INVALID ||
               (localtype->nlocals == 0 && paramtype->ntypes == 0));

        /*
         * Note: params can be in ctx->stack.
         * Be careful when resizing the later.
         */
        struct funcframe *frame;
        int ret;

        if (ctx->frames.lsize == ctx->options.max_frames) {
                return trap_with_id(ctx, TRAP_TOO_MANY_FRAMES,
                                    "too many frames");
        }
        ret = VEC_PREALLOC(ctx->frames, 1);
        if (ret != 0) {
                return ret;
        }
        const uint32_t nparams = resulttype_cellsize(paramtype);
        const uint32_t func_nlocals = localtype_cellsize(localtype);
        const uint32_t nlocals = nparams + func_nlocals;
        frame = &VEC_NEXTELEM(ctx->frames);
        frame->instance = inst;
        frame->funcidx = funcidx;
        frame->nresults = nresults;
        frame->labelidx = ctx->labels.lsize;
#if defined(TOYWASM_USE_SEPARATE_LOCALS)
        frame->localidx = ctx->locals.lsize;
        ret = VEC_PREALLOC(ctx->locals, nlocals);
        if (ret != 0) {
                return ret;
        }
#endif
        if (ei->maxlabels > 1) {
                frame->labelidx = ctx->labels.lsize;
                ret = VEC_PREALLOC(ctx->labels, ei->maxlabels - 1);
                if (ret != 0) {
                        return ret;
                }
        }

        if (ctx->frames.lsize > 0) {
                /*
                 * Note: ctx->instance can be different from
                 * frame->instance here.
                 */
                frame->callerpc = ptr2pc(ctx->instance->module, ctx->p);
        } else {
                /*
                 * Note: callerpc of the first frame is unused right now.
                 * Poison it with an invalid value to ensure no one is
                 * relying on the value.
                 */
#if !defined(NDEBUG)
                frame->callerpc = 0xdeadbeef;
#endif
        }
        frame->height = ctx->stack.lsize;
#if defined(TOYWASM_USE_SEPARATE_LOCALS)
        struct cell *locals = &VEC_ELEM(ctx->locals, frame->localidx);
        cells_copy(locals, params, nparams);

        /*
         * As we've copied "params" above, now it's safe to resize
         * stack.
         */
        ret = stack_prealloc(ctx, ei->maxcells);
        if (ret != 0) {
                return ret;
        }
#else
        const bool params_on_stack = params == &VEC_NEXTELEM(ctx->stack);

        ret = stack_prealloc(ctx, nlocals + ei->maxcells);
        if (ret != 0) {
                return ret;
        }

        struct cell *locals = &VEC_NEXTELEM(ctx->stack);
        if (params_on_stack) {
                xlog_trace_insn("params on stack");
                /* params are already in place */
        } else {
                xlog_trace_insn("copying %" PRIu32 " params", nparams);
                cells_copy(locals, params, nparams);
        }
#endif
        cells_zero(locals + nparams, nlocals - nparams);

        xlog_trace_insn("frame enter: maxlabels %u maxcells %u", ei->maxlabels,
                        ei->maxcells);
        uint32_t i;
        for (i = 0; i < nlocals; i++) {
                if (i == nparams) {
                        xlog_trace_insn("-- ^-params v-locals");
                }
#if defined(TOYWASM_USE_SMALL_CELLS)
                xlog_trace_insn("local [%" PRIu32 "] %08" PRIx32, i,
                                frame_locals(ctx, frame)[i].x);
#else
#if defined(TOYWASM_ENABLE_WASM_SIMD)
                xlog_trace_insn("local [%" PRIu32 "] %08" PRIx64 "%08" PRIx64,
                                i, frame_locals(ctx, frame)[i].x[1],
                                frame_locals(ctx, frame)[i].x[0]);
#else
                xlog_trace_insn("local [%" PRIu32 "] %08" PRIx64, i,
                                frame_locals(ctx, frame)[i].x);
#endif
#endif
        }

        /*
         * commit changes.
         */
        ctx->frames.lsize++;
#if defined(TOYWASM_USE_SEPARATE_LOCALS)
        assert(ctx->locals.lsize + nlocals <= ctx->locals.psize);
        ctx->locals.lsize += nlocals;
#else
        assert(ctx->stack.lsize + nlocals + ei->maxcells <= ctx->stack.psize);
        ctx->stack.lsize += nlocals;
#endif
        set_current_frame(ctx, frame, ei);
        assert(ctx->ei == ei);
        return 0;
}

void
frame_exit(struct exec_context *ctx)
{
        /*
         * Note: it's caller's responsibility to move results
         * on the operand stack if necessary.
         *
         * Note: while this function pops a frame from ctx->frames,
         * it leaves the contents of the frame intact.
         * Some of the callers actually rely on the behavior and use
         * the frame after calling this function.
         */
        struct funcframe *frame;
        assert(ctx->frames.lsize > 0);
        frame = VEC_POP(ctx->frames);
        assert(ctx->instance == frame->instance);
#if defined(TOYWASM_USE_LOCALS_CACHE)
        assert(ctx->current_locals == frame_locals(ctx, frame));
#endif
        if (ctx->frames.lsize > 0) {
                const struct funcframe *pframe = &VEC_LASTELEM(ctx->frames);
                set_current_frame(ctx, pframe, NULL);
                /*
                 * Note: frame->callerpc belongs to the module of pframe,
                 * which we have just restored by the set_current_frame above.
                 */
                ctx->p = pc2ptr(ctx->instance->module, frame->callerpc);
        }
        assert(frame->labelidx <= ctx->labels.lsize);
        ctx->labels.lsize = frame->labelidx;
#if defined(TOYWASM_USE_SEPARATE_LOCALS)
        assert(frame->localidx <= ctx->locals.lsize);
        ctx->locals.lsize = frame->localidx;
#endif
}

static const struct jump *
jump_table_lookup(const struct expr_exec_info *ei, uint32_t blockpc)
{
        uint32_t left = 0;
        uint32_t right = ei->njumps;
#if defined(TOYWASM_USE_JUMP_BINARY_SEARCH)
        /*
         * REVISIT: is it worth to switch to linear search for
         * small tables/range?
         */
        while (true) {
                assert(left < right);
                uint32_t mid = (left + right) / 2;
                const struct jump *jump = &ei->jumps[mid];
                if (jump->pc == blockpc) {
                        return jump;
                }
                if (jump->pc < blockpc) {
                        left = mid + 1;
                } else {
                        right = mid;
                }
        }
#else  /* defined(TOYWASM_USE_JUMP_BINARY_SEARCH) */
        uint32_t i;
        for (i = left; i < right; i++) {
                const struct jump *jump = &ei->jumps[i];
                if (jump->pc == blockpc) {
                        return jump;
                }
        }
#endif /* defined(TOYWASM_USE_JUMP_BINARY_SEARCH) */
        assert(false);
}

static const struct jump *
jump_lookup(struct exec_context *ctx, const struct expr_exec_info *ei,
            uint32_t blockpc)
{
        const struct jump *jump;
#if defined(TOYWASM_USE_JUMP_CACHE)
        jump = ctx->jump_cache;
        if (jump != NULL && jump->pc == blockpc) {
                STAT_INC(ctx, jump_cache_hit);
                return jump;
        }
#endif
        STAT_INC(ctx, jump_table_search);
        jump = jump_table_lookup(ei, blockpc);
#if defined(TOYWASM_USE_JUMP_CACHE)
        ctx->jump_cache = jump;
#endif
        return jump;
}

const struct func *
funcinst_func(const struct funcinst *fi)
{
        assert(!fi->is_host);
        const struct instance *inst = fi->u.wasm.instance;
        uint32_t funcidx = fi->u.wasm.funcidx;
        /*
         * Note: We do never create multiple funcinst for a func.
         * When re-exporting a function, we share the funcinst
         * from the original instance directly.
         */
        assert(VEC_ELEM(inst->funcs, funcidx) == fi);
        const struct module *m = inst->module;
        assert(funcidx >= m->nimportedfuncs);
        assert(funcidx < m->nimportedfuncs + m->nfuncs);
        return &m->funcs[funcidx - m->nimportedfuncs];
}

static int
do_wasm_call(struct exec_context *ctx, const struct funcinst *finst)
{
        int ret;
        const struct functype *type = funcinst_functype(finst);
        struct instance *callee_inst = finst->u.wasm.instance;
        const struct func *func = funcinst_func(finst);
        uint32_t nparams = resulttype_cellsize(&type->parameter);
        uint32_t nresults = resulttype_cellsize(&type->result);
        assert(ctx->stack.lsize >= nparams);
        ctx->stack.lsize -= nparams;
        ret = frame_enter(ctx, callee_inst, finst->u.wasm.funcidx, &func->e.ei,
                          &func->localtype, &type->parameter, nresults,
                          &VEC_NEXTELEM(ctx->stack));
        if (ret != 0) {
                return ret;
        }
        ctx->p = func->e.start;
        return 0;
}

static int
do_host_call(struct exec_context *ctx, const struct funcinst *finst)
{
        const struct functype *ft = funcinst_functype(finst);
        uint32_t nparams = resulttype_cellsize(&ft->parameter);
        uint32_t nresults = resulttype_cellsize(&ft->result);
        int ret;
        assert(ctx->stack.lsize >= nparams);
        if (nresults > nparams) {
                ret = stack_prealloc(ctx, nresults - nparams);
                if (ret != 0) {
                        return ret;
                }
        }
        struct cell *p = &VEC_ELEM(ctx->stack, ctx->stack.lsize - nparams);
        ret = finst->u.host.func(ctx, finst->u.host.instance, ft, p, p);
        assert(IS_RESTARTABLE(ret) || restart_info_is_none(ctx));
        if (ret != 0) {
                if (IS_RESTARTABLE(ret)) {
                        /*
                         * Note: it's a responsibility of host functions
                         * to keep function arguments on the stack intact
                         * when returning a restartable error.
                         */
                        STAT_INC(ctx, call_restart);
                }
                return ret;
        }
        ctx->stack.lsize -= nparams;
        ctx->stack.lsize += nresults;
        assert(ctx->stack.lsize <= ctx->stack.psize);
        return 0;
}

static int
do_call(struct exec_context *ctx, const struct funcinst *finst)
{
        STAT_INC(ctx, call);
        if (finst->is_host) {
                STAT_INC(ctx, host_call);
                return do_host_call(ctx, finst);
        } else {
                return do_wasm_call(ctx, finst);
        }
}

#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
static int
do_return_call(struct exec_context *ctx, const struct funcinst *finst)
{
        struct funcframe *frame = &VEC_LASTELEM(ctx->frames);
        uint32_t height = frame->height;
        frame_exit(ctx);
        frame_clear(frame);

        const struct functype *ft = funcinst_functype(finst);
        uint32_t arity = resulttype_cellsize(&ft->parameter);
        rewind_stack(ctx, height, arity);

        STAT_INC(ctx, tail_call);
        if (finst->is_host) {
                STAT_INC(ctx, host_tail_call);
        }
        return do_call(ctx, finst);
}
#endif /* defined(TOYWASM_ENABLE_WASM_TAILCALL) */

#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
static int
compare_taginst(const struct taginst *a, const struct taginst *b)
{
        /*
         * Note: in this implementation, tag equality is same as
         * pointer equality of taginst.
         */
        return a != b;
}

/*
 * find_catch: find the matching exception handler for the given taginst.
 *
 * At this point, we avoid modifying exec_context so that we can give
 * a better diagnostic on uncaught exception.
 */
static int
find_catch(const struct exec_context *ctx, const struct taginst *taginst,
           uint32_t *frameidxp, uint32_t *labelidxp, bool *allp)
{
        /* TODO: reject or support host frames */
        assert(ctx->frames.lsize > 0);
        uint32_t frameidx = ctx->frames.lsize - 1;
        uint32_t labelheight = ctx->labels.lsize;
        uint32_t catch_labelidx;
        bool all;
        uint32_t i;
        const struct funcframe *frame;
        for (i = 0; i < ctx->labels.lsize; i++) {
                uint32_t labelidx = ctx->labels.lsize - i - 1;
                do {
                        frame = &VEC_ELEM(ctx->frames, frameidx);
                        if (frame->labelidx <= labelidx) {
                                break;
                        }
                        labelheight = frame->labelidx;
                        assert(frameidx > 0);
                        frameidx--;
                } while (true);
                assert(labelidx >= frame->labelidx);
                assert(labelidx < labelheight - frame->labelidx);
                const struct instance *inst = frame->instance;
                const struct module *m = inst->module;
                const struct label *l = &VEC_ELEM(ctx->labels, labelidx);
                uint32_t blockpc = l->pc;
                xlog_trace_insn("%s: looking at frame %" PRIu32
                                " label %" PRIu32 " pc %06" PRIx32,
                                __func__, frameidx, labelidx, blockpc);
                const uint8_t *const blockp = pc2ptr(m, blockpc);
                const uint8_t *p = blockp;
                const uint8_t op = *p++;
                if (op != FRAME_OP_TRY_TABLE) {
                        xlog_trace_insn("%s: not a try-table", __func__);
                        continue;
                }
                /* const int64_t blocktype = */ read_leb_s33_nocheck(&p);
                const uint32_t vec_count = read_leb_u32_nocheck(&p);
                xlog_trace_insn("%s: try-table with %" PRIu32
                                " catch clause(s)",
                                __func__, vec_count);
                uint32_t j;
                for (j = 0; j < vec_count; j++) {
                        xlog_trace_insn(
                                "%s: looking at catch at pc %06" PRIx32,
                                __func__, ptr2pc(m, p));
                        const uint8_t catch_op = *p++;
                        uint32_t catch_tagidx;
                        const struct taginst *catch_taginst = NULL;
                        switch (catch_op) {
                        case CATCH_REF:
                        case CATCH:
                                catch_tagidx = read_leb_u32_nocheck(&p);
                                assert(catch_tagidx <
                                       m->nimportedtags + m->ntags);
                                catch_taginst =
                                        VEC_ELEM(inst->tags, catch_tagidx);
                                break;
                        case CATCH_ALL_REF:
                        case CATCH_ALL:
                                break;
                        default:
                                assert(false);
                        }
                        uint32_t catch_label = read_leb_u32_nocheck(&p);
                        /* labelidx here is of try_table block. */
                        assert(catch_label <= labelidx - frame->labelidx);
                        catch_labelidx =
                                catch_label + (labelheight - labelidx);
                        if (catch_taginst == NULL) {
                                all = true;
                                goto found;
                        }
                        if (!compare_taginst(catch_taginst, taginst)) {
                                all = false;
                                goto found;
                        }
                }
        }
        /* not found */
        return ENOENT;

found:
        *frameidxp = frameidx;
        *labelidxp = catch_labelidx;
        *allp = all;
        return 0;
}

static int
do_exception(struct exec_context *ctx)
{
        /*
         * pop an exception on the top of the stack.
         * cf. push_exception
         */
        uint32_t exnref_csz = valtype_cellsize(TYPE_EXNREF);
        assert(ctx->stack.lsize >= exnref_csz);
        const struct cell *exc_cells =
                &VEC_ELEM(ctx->stack, ctx->stack.lsize - exnref_csz);
        const struct exception *exc = (const void *)exc_cells;
        ctx->stack.lsize -= exnref_csz;

        const struct taginst *taginst;
        /* Note: use memcpy as exc might be misaligned */
        memcpy(&taginst, exception_tag_ptr(exc), sizeof(taginst));
        xlog_trace_insn("%s: taginst %p", __func__, (const void *)taginst);

        /*
         * find the matching catch clause
         */
        uint32_t frameidx;
        uint32_t labelidx;
        bool all;
        int ret = find_catch(ctx, taginst, &frameidx, &labelidx, &all);
        if (ret != 0) {
                xlog_trace_insn("%s: no catch clause found for tag", __func__);
                return trap_with_id(ctx, TRAP_UNCAUGHT_EXCEPTION,
                                    "uncaught exception");
        }
        xlog_trace_insn("%s: a catch clause found at frame %" PRIu32
                        " label %" PRIu32 " for tag",
                        __func__, frameidx, labelidx);

        /*
         * rewind the frames
         */
        while (frameidx + 1 < ctx->frames.lsize) {
                struct funcframe *frame = &VEC_LASTELEM(ctx->frames);
                frame_exit(ctx);
                frame_clear(frame);
        }
        assert(frameidx + 1 == ctx->frames.lsize);
        /*
         * jump to the label.
         * REVISIT: share some code with do_branch?
         */
        uint32_t height;
        uint32_t arity;
        assert(labelidx <= ctx->labels.lsize);
        struct funcframe *frame = &VEC_LASTELEM(ctx->frames);
        if (ctx->labels.lsize - labelidx == frame->labelidx) {
                frame_exit(ctx);
                height = frame->height;
                arity = frame->nresults;
                frame_clear(frame);
        } else {
                bool in_block =
                        branch_to_label(ctx, labelidx, false, &height, &arity);
                assert(!in_block);
        }

        /*
         * rewind the operand stack similarly to rewind_stack.
         * but use the values from the exception.
         */
        xlog_trace_insn("%s: rewinding operand stack: height %" PRIu32
                        " -> %" PRIu32,
                        __func__, ctx->stack.lsize, height);
        uint32_t csz;
        if (all) {
                csz = 0;
        } else {
                const struct functype *ft = taginst_functype(taginst);
                const struct resulttype *rt = &ft->parameter;
                csz = resulttype_cellsize(rt);
        }
        xlog_trace_insn("%s: csz %" PRIu32, __func__, csz);
        if (arity != csz) {
                /* arity != csz here means catch_ref/catch_ref_all. */
                assert(arity == csz + exnref_csz);
        }

        /*
         * Note: we use cells_move here as src and dst can overlap.
         */
        struct cell *dst = &VEC_ELEM(ctx->stack, height);
        if (arity != csz) {
                /* move exc to the new location first */
                assert(dst + csz >= exc_cells);
                cells_move(dst + csz, exc_cells, exnref_csz);
                exc_cells = dst + csz;
                exc = (const void *)exc_cells;
        }
        cells_move(dst, exc_cells, csz);
        ctx->stack.lsize = height + arity;
        xlog_trace_insn("%s: copied csz %" PRIu32, __func__, csz);
        return 0;
}
#endif /* defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING) */

/*
 * a bit shrinked version of get_functype_for_blocktype.
 * get the number of struct cell for parameters and results.
 */
static void
get_arity_for_blocktype(const struct module *m, int64_t blocktype,
                        uint32_t *parameter, uint32_t *result)
{
        if (blocktype < 0) {
                uint8_t u8 = (uint8_t)(blocktype & 0x7f);
                if (u8 == 0x40) {
                        *parameter = 0;
                        *result = 0;
                        return;
                }
                assert(is_valtype(u8));
                *parameter = 0;
                *result = valtype_cellsize(u8);
                return;
        }
        assert(blocktype <= UINT32_MAX);
        assert(blocktype < m->ntypes);
        const struct functype *ft = &m->types[blocktype];
        *parameter = resulttype_cellsize(&ft->parameter);
        *result = resulttype_cellsize(&ft->result);
}

void
rewind_stack(struct exec_context *ctx, uint32_t height, uint32_t arity)
{
        /*
         * rewind the operand stack. (to `height`)
         * moving the return values. (of `arity`)
         *
         * x x x y y y r r
         *             ---
         *             arity
         *
         *       <-----
         *       rewind
         *
         * x x x r r
         *       ^   ^
         *       |   |
         *    height |
         *           |
         *          new stack lsize
         */
        assert(height <= ctx->stack.lsize);
        assert(arity <= ctx->stack.lsize);
        assert(height + arity <= ctx->stack.lsize);
        if (height + arity == ctx->stack.lsize) {
                return;
        }
        cells_move(&VEC_ELEM(ctx->stack, height),
                   &VEC_ELEM(ctx->stack, ctx->stack.lsize - arity), arity);
        ctx->stack.lsize = height + arity;
}

#if TOYWASM_JUMP_CACHE2_SIZE > 0
static const struct jump_cache *
jump_cache2_lookup(struct exec_context *ctx, uint32_t blockpc, bool goto_else)
{
        uint32_t key = blockpc + goto_else;
        const struct jump_cache *cache =
                &ctx->cache[key % ARRAYCOUNT(ctx->cache)];
        if (cache->key == key) {
                return cache;
        }
        return NULL;
}

static void
jump_cache2_store(struct exec_context *ctx, uint32_t blockpc, bool goto_else,
                  bool stay_in_block, uint32_t param_arity, uint32_t arity,
                  const uint8_t *p)
{
        uint32_t key = blockpc + goto_else;
        struct jump_cache *cache = &ctx->cache[key % ARRAYCOUNT(ctx->cache)];
        assert(cache->key != key);
        cache->key = key;
        cache->stay_in_block = stay_in_block;
        cache->param_arity = param_arity;
        cache->arity = arity;
        cache->target = p;
}
#endif

/*
 * in case of goto_else=true, returns true if it was a jump inside a block.
 * otherwise always returns false.
 */
static bool
block_exit(struct exec_context *ctx, uint32_t blockpc, bool goto_else,
           uint32_t *param_arityp, uint32_t *arityp)
{
        /*
         * exit from a block.
         *
         * "exit" here might be misleading. in the case of "loop" block,
         * it actually loops.
         *
         * Note: This is a bit complicated because we (ab)use
         * this for the "if" opcode as well and an "if" might or
         * might not have "else".
         * If an "if" has "else", the "stay_in_block" logic is
         * used. In that case, we should keep the label intact.
         * Otherwise, it's same as "br 0".
         *
         * Note: While the jump table is useful for forward jump,
         * it isn't for backward jump. (loop)
         * For a random wasm modules I happened to have, it seems
         * 5-10% of jump table entries are for "loop".
         */

        /*
         * parse the block op to check
         * - if it's a "loop"
         * - blocktype
         */
        const struct module *const m = ctx->instance->module;
        const uint8_t *const blockp = pc2ptr(m, blockpc);
        const uint8_t *p = blockp;
        const uint8_t op = *p++;
        assert(op == FRAME_OP_LOOP || op == FRAME_OP_IF ||
               op == FRAME_OP_BLOCK);
        uint32_t param_arity;
        uint32_t arity;
        if (op != FRAME_OP_LOOP) {
                /*
                 * do a jump. (w/ jump table)
                 */
                const struct expr_exec_info *const ei = ctx->ei;
                if (ei->jumps != NULL) {
                        xlog_trace_insn("jump w/ table");
                        bool stay_in_block = false;
                        const struct jump *jump;
                        jump = jump_lookup(ctx, ei, blockpc);
                        if (goto_else) {
                                const struct jump *jump_to_else = jump + 1;
                                assert(jump_to_else->pc == blockpc + 1);
                                if (jump_to_else->targetpc != 0) {
                                        stay_in_block = true;
                                        jump = jump_to_else;
                                }
                        }
                        assert(jump->targetpc != 0);
                        ctx->p = pc2ptr(ctx->instance->module, jump->targetpc);
                        if (stay_in_block) {
                                xlog_trace_insn("jump inside a block");
                                return true;
                        }
                }

                const int64_t blocktype = read_leb_s33_nocheck(&p);

                /*
                 * do a jump. (w/o jump table)
                 */
                if (ei->jumps == NULL) {
                        xlog_trace_insn("jump w/o table");
                        /*
                         * The only way to find out the jump target is
                         * to parse every instructions. This is expensive.
                         *
                         * REVISIT: skipping LEBs can be optimized better
                         * than the current code.
                         */
                        bool stay_in_block = skip_expr(&p, goto_else);
                        ctx->p = p;
                        if (stay_in_block) {
                                return true;
                        }
                }
                get_arity_for_blocktype(m, blocktype, &param_arity, &arity);
        } else {
                STAT_INC(ctx, jump_loop);
                const int64_t blocktype = read_leb_s33_nocheck(&p);
                get_arity_for_blocktype(m, blocktype, &param_arity, &arity);
                ctx->p = blockp;
                arity = param_arity;
        }
        *param_arityp = param_arity;
        *arityp = arity;
        return false;
}

/*
 * a cached version of block_exit.
 * the parameters and return values are same as block_exit.
 */
static bool
cached_block_exit(struct exec_context *ctx, uint32_t blockpc, bool goto_else,
                  uint32_t *param_arityp, uint32_t *arityp)
{
        uint32_t param_arity;
        uint32_t arity;
#if TOYWASM_JUMP_CACHE2_SIZE > 0
        const struct jump_cache *cache;
        if ((cache = jump_cache2_lookup(ctx, blockpc, goto_else)) != NULL) {
                STAT_INC(ctx, jump_cache2_hit);
                ctx->p = cache->target;
                if (cache->stay_in_block) {
                        assert(cache->param_arity == 0);
                        assert(cache->arity == 0);
                        return true;
                }
                param_arity = cache->param_arity;
                arity = cache->arity;
        } else
#endif
        {
                if (block_exit(ctx, blockpc, goto_else, &param_arity,
                               &arity)) {
#if TOYWASM_JUMP_CACHE2_SIZE > 0
                        jump_cache2_store(ctx, blockpc, goto_else, true, 0, 0,
                                          ctx->p);
#endif
                        return true;
                }
#if TOYWASM_JUMP_CACHE2_SIZE > 0
                jump_cache2_store(ctx, blockpc, goto_else, false, param_arity,
                                  arity, ctx->p);
#endif
        }
        *param_arityp = param_arity;
        *arityp = arity;
        return false;
}

static bool
branch_to_label(struct exec_context *ctx, uint32_t labelidx, bool goto_else,
                uint32_t *heightp, uint32_t *arityp)
{
        /*
         * Jump to a label.
         *
         * A label points to the corresponding block-starting opcode.
         * (block, loop, if, ...)
         */
        const struct label *l =
                &VEC_ELEM(ctx->labels, ctx->labels.lsize - labelidx - 1);
        uint32_t blockpc = l->pc;
        uint32_t arity;
        uint32_t param_arity;
        if (cached_block_exit(ctx, blockpc, goto_else, &param_arity, &arity)) {
                return true;
        }
        ctx->labels.lsize -= labelidx + 1;
        /*
         * Note: The spec says to pop the values before
         * pushing the label, which we don't do.
         * Instead, we adjust the height accordingly here.
         */
        assert(l->height >= param_arity);
        uint32_t height = l->height - param_arity;

        *heightp = height;
        *arityp = arity;
        return false;
}

static void
do_branch(struct exec_context *ctx, uint32_t labelidx, bool goto_else)
{
        struct funcframe *frame = &VEC_LASTELEM(ctx->frames);
        assert(labelidx == 0 || !goto_else);
        assert(ctx->labels.lsize >= frame->labelidx);
        assert(labelidx <= ctx->labels.lsize - frame->labelidx);
        uint32_t height;
        uint32_t arity; /* arity of the label */
        if (goto_else) {
                STAT_INC(ctx, branch_goto_else);
        } else {
                STAT_INC(ctx, branch);
        }
        if (ctx->labels.lsize - labelidx == frame->labelidx) {
                /*
                 * Exit the function.
                 */
                xlog_trace_insn("do_branch: exiting function");
                frame_exit(ctx);
                height = frame->height;
                arity = frame->nresults;
                frame_clear(frame);
        } else {
                if (branch_to_label(ctx, labelidx, goto_else, &height,
                                    &arity)) {
                        return;
                }
        }

        rewind_stack(ctx, height, arity);
}

int
fetch_exec_next_insn(const uint8_t *p, struct cell *stack,
                     struct exec_context *ctx)
{
#if !(defined(TOYWASM_USE_SEPARATE_EXECUTE) && defined(TOYWASM_USE_TAILCALL))
        assert(ctx->p == p);
#endif
        assert(ctx->event == EXEC_EVENT_NONE);
        assert(ctx->frames.lsize > 0);
#if defined(TOYWASM_ENABLE_TRACING_INSN)
        uint32_t pc = ptr2pc(ctx->instance->module, p);
#endif
        uint32_t op = *p++;
#if defined(TOYWASM_USE_SEPARATE_EXECUTE)
        xlog_trace_insn("exec %06" PRIx32 ": %s (%02" PRIx32 ")", pc,
                        instructions[op].name, op);
        const struct exec_instruction_desc *desc = &exec_instructions[op];
#if defined(TOYWASM_USE_TAILCALL)
        __musttail
#endif
                return desc->fetch_exec(p, stack, ctx);
#else
        const struct instruction_desc *desc = &instructions[op];
        if (__predict_false(desc->next_table != NULL)) {
                op = read_leb_u32_nocheck(&p);
                desc = &desc->next_table[op];
        }
        xlog_trace_insn("exec %06" PRIx32 ": %s", pc, desc->name);
        assert(desc->process != NULL);
        struct context common_ctx;
        memset(&common_ctx, 0, sizeof(common_ctx));
        common_ctx.exec = ctx;
        ctx->p = p;
        return desc->process(&ctx->p, NULL, &common_ctx);
#endif
}

static int
restart_insn(struct exec_context *ctx)
{
        assert(ctx->event == EXEC_EVENT_RESTART_INSN);
        ctx->event = EXEC_EVENT_NONE;
        xlog_trace("%s: restarting insn at %" PRIx32, __func__,
                   ptr2pc(ctx->instance->module, ctx->p));
#if defined(TOYWASM_USE_SEPARATE_EXECUTE)
        struct cell *stack = &VEC_NEXTELEM(ctx->stack);
        return ctx->event_u.restart_insn.fetch_exec(ctx->p, stack, ctx);
#else
        struct context common_ctx;
        memset(&common_ctx, 0, sizeof(common_ctx));
        common_ctx.exec = ctx;
        return ctx->event_u.restart_insn.process(&ctx->p, NULL, &common_ctx);
#endif
}

int
check_interrupt(struct exec_context *ctx)
{
        /*
         * theoretically we probably need a memory barrier.
         * practically it shouldn't be a problem though.
         */
        if (ctx->intrp != NULL && __predict_false(*ctx->intrp != 0)) {
                if (ctx->user_intr_delay_count < ctx->user_intr_delay) {
                        ctx->user_intr_delay_count++;
                } else {
                        ctx->user_intr_delay_count = 0;
                        xlog_trace("get interrupt");
                        STAT_INC(ctx, interrupt_user);
                        return ETOYWASMUSERINTERRUPT;
                }
        }
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        if (ctx->cluster != NULL) {
                int ret = cluster_check_interrupt(ctx, ctx->cluster);
                if (ret != 0) {
                        return ret;
                }
        }
#endif /* defined(TOYWASM_ENABLE_WASM_THREADS) */
#if defined(TOYWASM_USE_USER_SCHED)
        if (ctx->sched != NULL && sched_need_resched(ctx->sched)) {
                xlog_trace("%s: need resched ctx %p", __func__, (void *)ctx);
                STAT_INC(ctx, interrupt_usched);
                return ETOYWASMRESTART;
        }
#else /* defined(TOYWASM_USE_USER_SCHED) */

#undef TSAN
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define TSAN
#endif
#endif

#if !defined(TSAN) && !defined(NDEBUG) /* a bit abuse of NDEBUG */
        /* inject artificial restart events to test restart logic. */
        static int x = 0;
        x++;
        if ((x % 10) == 0) {
                STAT_INC(ctx, interrupt_debug);
                return ETOYWASMRESTART;
        }
#endif
#endif /* defined(TOYWASM_USE_USER_SCHED) */
        return 0;
}

int
check_interrupt_interval_ms(struct exec_context *ctx)
{
#if defined(TOYWASM_USE_USER_SCHED) || !defined(TOYWASM_PREALLOC_SHARED_MEMORY)
        /* use shorter interval for userland thread */
        int interval_ms = 50;
#else
        int interval_ms = 300;
#endif
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        struct cluster *c = ctx->cluster;
        if (c != NULL) {
                /*
                 * try to avoid putting too much pressure on the system.
                 *
                 *   checks_per_sec = (1000 ms / interval_ms) * nrunners
                 *   interval_ms = (1000 ms / checks_per_sec) * nrunners
                 */
                const uint32_t nrunners = c->nrunners; /* XXX lock */
                const int max_checks_per_sec = 100;
                const int max_interval_ms = 5000;
                /*
                 * checks_per_sec = (1000 / interval_ms) * nrunners
                 *                > max_checks_per_sec
                 */
                if (nrunners > max_checks_per_sec / (1000 / interval_ms)) {
                        interval_ms = (1000 / max_checks_per_sec) * nrunners;
                        if (interval_ms > max_interval_ms) {
                                interval_ms = max_interval_ms;
                        }
                }
        }
#endif
        return interval_ms;
}

#define CHECK_INTERVAL_MAX UINT32_MAX
#define CHECK_INTERVAL_DEFAULT 1000
#define CHECK_INTERVAL_MIN 1

static void
adjust_check_interval(struct exec_context *ctx, const struct timespec *now,
                      const struct timespec *last)
{
        struct timespec diff;
        timespec_sub(now, last, &diff);
        uint64_t diff_ms = timespec_to_ms(&diff);
        uint32_t check_interval = ctx->check_interval;
        uint32_t interval_ms = check_interrupt_interval_ms(ctx);
        if (diff_ms < interval_ms / 2) {
                if (check_interval <= CHECK_INTERVAL_MAX / 2) {
                        check_interval *= 2;
                } else {
                        check_interval = CHECK_INTERVAL_MAX;
                }
        } else if (diff_ms / 2 > interval_ms) {
                check_interval /= 2;
                if (check_interval < CHECK_INTERVAL_MIN) {
                        check_interval = CHECK_INTERVAL_MIN;
                }
        }
        xlog_trace("check_interval %" PRIu32, check_interval);
        ctx->check_interval = check_interval;
}

/*
 * REVISIT: probably it's cleaner to integrate into frame_exit
 */
static void
return_to_hostfunc(struct exec_context *ctx)
{
        assert(ctx->frames.lsize == ctx->bottom);
        assert(ctx->restarts.lsize > 0);
        struct restart_info *restart = VEC_POP(ctx->restarts);
        assert(restart->restart_type == RESTART_HOSTFUNC);
        struct restart_hostfunc *hf = &restart->restart_u.hostfunc;
        assert(ctx->bottom >= hf->saved_bottom);
        assert(ctx->stack.lsize >= hf->stack_adj);
        ctx->bottom = hf->saved_bottom;
        ctx->stack.lsize -= hf->stack_adj;
        ctx->event_u.call.func = hf->func;
        ctx->event = EXEC_EVENT_CALL;
}

int
exec_expr(uint32_t funcidx, const struct expr *expr,
          const struct localtype *localtype,
          const struct resulttype *parametertype, uint32_t nresults,
          const struct cell *params, struct exec_context *ctx)
{
        int ret;

        assert(ctx->instance != NULL);
        assert(ctx->instance->module != NULL);
        ret = frame_enter(ctx, ctx->instance, funcidx, &expr->ei, localtype,
                          parametertype, nresults, params);
        if (ret != 0) {
                return ret;
        }
        ctx->p = expr->start;
        return exec_expr_continue(ctx);
}

int
exec_expr_continue(struct exec_context *ctx)
{
        struct timespec last;
        bool has_last = false;
        uint32_t n = ctx->check_interval;
        assert(n > 0);
        while (true) {
                int ret;
                switch (ctx->event) {
#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
                case EXEC_EVENT_RETURN_CALL:
                        assert(ctx->frames.lsize > 0);
                        ret = do_return_call(ctx, ctx->event_u.call.func);
                        if (ret != 0) {
                                if (IS_RESTARTABLE(ret)) {
                                        ctx->event = EXEC_EVENT_CALL;
                                        /*
                                         * Note: because we have changed
                                         * ctx->event above, only the first
                                         * restart is counted as
                                         * tail_call_restart.
                                         */
                                        STAT_INC(ctx, tail_call_restart);
                                }
                                return ret;
                        }
                        break;
#endif /* defined(TOYWASM_ENABLE_WASM_TAILCALL) */
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
                case EXEC_EVENT_EXCEPTION:
                        ret = do_exception(ctx);
                        if (ret != 0) {
                                return ret;
                        }
                        break;
#endif /* defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING) */
                case EXEC_EVENT_CALL:
                        ret = do_call(ctx, ctx->event_u.call.func);
                        if (ret != 0) {
                                return ret;
                        }
                        break;
                case EXEC_EVENT_BRANCH:
                        assert(ctx->frames.lsize > 0);
                        do_branch(ctx, ctx->event_u.branch.index,
                                  ctx->event_u.branch.goto_else);
                        break;
                case EXEC_EVENT_RESTART_INSN:
                        /*
                         * While it's possible to have a more generic
                         * instruction restart logic by pushing back PC,
                         * it's a bit tricky with the way how we implement
                         * instruction fetch. A naive implementation would
                         * make fetch_exec_next_insn save the PC for
                         * possible restart. But it can be a bit expensive
                         * comparing to what fetch_exec_next_insn currently
                         * does.
                         *
                         * Note: it isn't simple as "PC -= insn_len"
                         * because wasm opcodes have variable length.
                         * Because multibyte opcodes even have redundant
                         * encodings, it's basically impossible to parse
                         * the instruction backward.
                         *
                         * Instead, this implementation relies on the
                         * opcode functions set up an explicit execution
                         * event when returning a restartable error.
                         */
                        ret = restart_insn(ctx);
                        goto after_insn;
                case EXEC_EVENT_NONE:
                        break;
                }
                ctx->event = EXEC_EVENT_NONE;
                if (ctx->frames.lsize == ctx->bottom) {
                        if (ctx->restarts.lsize == 0) {
                                break;
                        }
                        return_to_hostfunc(ctx);
                        continue;
                }
                n--;
                if (__predict_false(n == 0)) {
                        struct timespec now;
                        ret = timespec_now(CLOCK_MONOTONIC, &now);
                        if (ret != 0) {
                                return ret;
                        }
                        if (has_last) {
                                adjust_check_interval(ctx, &now, &last);
                        }
                        last = now;
                        has_last = true;
                        ret = check_interrupt(ctx);
                        if (ret != 0) {
                                if (IS_RESTARTABLE(ret)) {
                                        STAT_INC(ctx, exec_loop_restart);
                                }
                                return ret;
                        }
                        n = ctx->check_interval;
                        assert(n > 0);
                }
                struct cell *stack = &VEC_NEXTELEM(ctx->stack);
                ret = fetch_exec_next_insn(ctx->p, stack, ctx);
after_insn:
                assert(IS_RESTARTABLE(ret) ==
                       (ctx->event == EXEC_EVENT_RESTART_INSN));
                if (ret != 0) {
                        if (ctx->trapped) {
                                xlog_trace("got a trap");
                        }
                        return ret;
                }
        }
        return 0;
}

int
exec_push_vals(struct exec_context *ctx, const struct resulttype *rt,
               const struct val *vals)
{
        uint32_t ncells = resulttype_cellsize(rt);
        int ret = stack_prealloc(ctx, ncells);
        if (ret != 0) {
                return ret;
        }
        struct cell *cells = &VEC_NEXTELEM(ctx->stack);
        vals_to_cells(vals, cells, rt);
        ctx->stack.lsize += ncells;
        return 0;
}

void
exec_pop_vals(struct exec_context *ctx, const struct resulttype *rt,
              struct val *vals)
{
        uint32_t ncells = resulttype_cellsize(rt);
        assert(ctx->stack.lsize >= ncells);
        ctx->stack.lsize -= ncells;
        const struct cell *cells = &VEC_NEXTELEM(ctx->stack);
        vals_from_cells(vals, cells, rt);
}

bool
skip_expr(const uint8_t **pp, bool goto_else)
{
        const uint8_t *p = *pp;
        struct context ctx;
        memset(&ctx, 0, sizeof(ctx));
        uint32_t block_level = 0;
        while (true) {
                uint32_t op = *p++;
                const struct instruction_desc *desc = &instructions[op];
                if (desc->next_table != NULL) {
                        uint32_t op2 = read_leb_u32_nocheck(&p);
                        desc = &desc->next_table[op2];
                }
                assert(desc->process != NULL);
                int ret = desc->process(&p, NULL, &ctx);
                assert(ret == 0);
                switch (op) {
                case FRAME_OP_BLOCK:
                case FRAME_OP_LOOP:
                case FRAME_OP_IF:
                        block_level++;
                        break;
                case FRAME_OP_ELSE:
                        if (goto_else && block_level == 0) {
                                *pp = p;
                                return true;
                        }
                        break;
                case FRAME_OP_END:
                        if (block_level == 0) {
                                *pp = p;
                                return false;
                        }
                        block_level--;
                        break;
                default:
                        break;
                }
        }
}

const struct resulttype g_empty_rt = {
        .ntypes = 0, .is_static = true, CELLIDX_NONE};

int
exec_const_expr(const struct expr *expr, enum valtype type, struct val *result,
                struct exec_context *ctx)
{
        uint32_t saved_height = ctx->frames.lsize;
        static const struct localtype no_locals = {
                .nlocals = 0,
                .nlocalchunks = 0,
                .localchunks = NULL,
#if defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
                .cellidx =
                        {
                                NULL,
                        },
#endif
        };
        int ret;
        uint32_t csz = valtype_cellsize(type);
        ret = exec_expr(FUNCIDX_INVALID, expr, &no_locals, empty_rt, csz, NULL,
                        ctx);
        /*
         * it's very unlikely for a const expr to use a restart.
         * but just in case.
         */
        while (IS_RESTARTABLE(ret)) {
                xlog_trace("%s: restarting execution of a const expr\n",
                           __func__);
                ret = exec_expr_continue(ctx);
        }
        if (ret != 0) {
                return ret;
        }
        DEFINE_RESULTTYPE(, rt, &type, 1);
        exec_pop_vals(ctx, &rt, result);
        assert(ctx->frames.lsize == saved_height);
        return 0;
}

void
exec_context_init(struct exec_context *ctx, struct instance *inst)
{
        memset(ctx, 0, sizeof(*ctx));
        ctx->instance = inst;
        report_init(&ctx->report0);
        ctx->report = &ctx->report0;
        ctx->check_interval = CHECK_INTERVAL_DEFAULT;
        exec_options_set_defaults(&ctx->options);
}

void
exec_context_clear(struct exec_context *ctx)
{
        struct funcframe *frame;
        VEC_FOREACH(frame, ctx->frames) {
                frame_clear(frame);
        }
        VEC_FREE(ctx->frames);
        VEC_FREE(ctx->stack);
        VEC_FREE(ctx->labels);
#if defined(TOYWASM_USE_SEPARATE_LOCALS)
        VEC_FREE(ctx->locals);
#endif
        VEC_FREE(ctx->restarts);
        report_clear(&ctx->report0);
        ctx->report = NULL;
}

uint32_t
find_type_annotation(struct exec_context *ctx, const uint8_t *p)
{
#if defined(TOYWASM_USE_SMALL_CELLS)
        const struct expr_exec_info *ei = ctx->ei;
        const struct type_annotations *an = &ei->type_annotations;
        assert(an->default_size > 0);
        if (an->ntypes == 0) {
                STAT_INC(ctx, type_annotation_lookup1);
                return an->default_size;
        }
        const uint32_t pc = ptr2pc(ctx->instance->module, p);
        uint32_t i;
        for (i = 0; i < an->ntypes; i++) {
                if (pc < an->types[i].pc) {
                        break;
                }
        }
        if (i == 0) {
                STAT_INC(ctx, type_annotation_lookup2);
                return an->default_size;
        }
        assert(an->types[i - 1].size > 0);
        STAT_INC(ctx, type_annotation_lookup3);
        return an->types[i - 1].size;
#else
        return 1;
#endif
}
