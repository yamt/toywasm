#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bitmap.h"
#include "cluster.h"
#include "context.h"
#include "exec.h"
#include "expr.h"
#include "insn.h"
#include "leb128.h"
#include "nbio.h"
#include "platform.h"
#include "shared_memory_impl.h"
#include "suspend.h"
#include "timeutil.h"
#include "type.h"
#include "usched.h"
#include "util.h"
#include "xlog.h"

int
vtrap(struct exec_context *ctx, enum trapid id, const char *fmt, va_list ap)
{
        assert(!ctx->trapped);
        ctx->trapped = true;
        ctx->trap.trapid = id;
        vreport(ctx->report, fmt, ap);
        xlog_trace("TRAP: %s", ctx->report->msg);
        return ETOYWASMTRAP;
}

int
trap_with_id(struct exec_context *ctx, enum trapid id, const char *fmt, ...)
{
        int ret;
        va_list ap;
        va_start(ap, fmt);
        ret = vtrap(ctx, id, fmt, ap);
        va_end(ap);
        return ret;
}

int
memory_getptr2(struct exec_context *ctx, uint32_t memidx, uint32_t ptr,
               uint32_t offset, uint32_t size, void **pp, bool *movedp)
{
        struct instance *inst = ctx->instance;
        struct meminst *meminst = VEC_ELEM(inst->mems, memidx);
        assert(meminst->allocated <=
               (uint64_t)meminst->size_in_pages * WASM_PAGE_SIZE);
        if (__predict_false(offset > UINT32_MAX - ptr)) {
                /*
                 * i failed to find this in the spec.
                 * but some of spec tests seem to test this.
                 */
                goto do_trap;
        }
        uint32_t ea = ptr + offset;
        if (__predict_false(size == 0)) {
                /*
                 * a zero-length access still needs address check.
                 * this can be either from host functions or
                 * bulk instructions like memory.copy.
                 */
                if (ea > 0 &&
                    (ea - 1) / WASM_PAGE_SIZE >= meminst->size_in_pages) {
                        goto do_trap;
                }
                goto success;
        }
        if (size - 1 > UINT32_MAX - ea) {
                goto do_trap;
        }
        uint32_t last_byte = ea + (size - 1);
        if (__predict_false(last_byte >= meminst->allocated)) {
                uint32_t need_in_pages = last_byte / WASM_PAGE_SIZE + 1;
                if (need_in_pages > meminst->size_in_pages) {
                        int ret;
do_trap:
                        ret = trap_with_id(
                                ctx, TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS,
                                "invalid memory access at %04" PRIx32
                                " %08" PRIx32 " + %08" PRIx32 ", size %" PRIu32
                                ", meminst size %" PRIu32,
                                memidx, ptr, offset, size,
                                meminst->size_in_pages);
                        assert(ret != 0); /* appease clang-tidy */
                        return ret;
                }
                /*
                 * Note: shared memories do never come here because
                 * we handle their growth in memory_grow.
                 */
                assert((meminst->type->flags & MEMTYPE_FLAG_SHARED) == 0);
#if SIZE_MAX <= UINT32_MAX
                if (last_byte >= SIZE_MAX) {
                        goto do_trap;
                }
#endif
                size_t need = (size_t)last_byte + 1;
                assert(need > meminst->allocated);
                int ret = resize_array((void **)&meminst->data, need, 1);
                if (ret != 0) {
                        return ret;
                }
                xlog_trace_insn("extend memory %" PRIu32 " from %zu to %zu",
                                memidx, meminst->allocated, need);
                if (movedp != NULL) {
                        *movedp = true;
                }
                memset(meminst->data + meminst->allocated, 0,
                       need - meminst->allocated);
                meminst->allocated = need;
        }
success:
        xlog_trace_insn("memory access: at %04" PRIx32 " %08" PRIx32
                        " + %08" PRIx32 ", size %" PRIu32
                        ", meminst size %" PRIu32,
                        memidx, ptr, offset, size, meminst->size_in_pages);
        *pp = meminst->data + ea;
        return 0;
}

int
memory_getptr(struct exec_context *ctx, uint32_t memidx, uint32_t ptr,
              uint32_t offset, uint32_t size, void **pp)
{
        return memory_getptr2(ctx, memidx, ptr, offset, size, pp, NULL);
}

#if defined(TOYWASM_ENABLE_WASM_THREADS)
int
memory_atomic_getptr(struct exec_context *ctx, uint32_t memidx, uint32_t ptr,
                     uint32_t offset, uint32_t size, void **pp,
                     struct toywasm_mutex **lockp)
        NO_THREAD_SAFETY_ANALYSIS /* conditionl lock */
{
        struct instance *inst = ctx->instance;
        struct meminst *meminst = VEC_ELEM(inst->mems, memidx);
        struct shared_meminst *shared = meminst->shared;
        struct toywasm_mutex *lock = NULL;
        if (shared != NULL && lockp != NULL) {
                lock = atomics_mutex_getptr(&shared->tab, ptr + offset);
                toywasm_mutex_lock(lock);
        }
        int ret;
        ret = memory_getptr(ctx, memidx, ptr, offset, size, pp);
        if (ret != 0) {
                goto fail;
        }
        if (((ptr + offset) % size) != 0) {
                ret = trap_with_id(ctx, TRAP_UNALIGNED_ATOMIC_OPERATION,
                                   "unaligned atomic");
                assert(ret != 0); /* appease clang-tidy */
                goto fail;
        }
        if (lockp != NULL) {
                *lockp = lock;
        }
        return 0;
fail:
        memory_atomic_unlock(lock);
        return ret;
}

void
memory_atomic_unlock(struct toywasm_mutex *lock)
        NO_THREAD_SAFETY_ANALYSIS /* conditionl lock */
{
        if (lock != NULL) {
                toywasm_mutex_unlock(lock);
        }
}
#endif

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

static int
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
                 */
                assert(ei != NULL);
                ctx->paramtype = NULL;
                ctx->localtype = NULL;
                ctx->ei = ei;
        } else {
                const struct module *m = inst->module;
                const struct functype *ft = module_functype(m, funcidx);
                const struct func *func =
                        &m->funcs[funcidx - m->nimportedfuncs];
                ctx->paramtype = &ft->parameter;
                assert(frame->nresults == resulttype_cellsize(&ft->result));
                ctx->localtype = &func->localtype;
                assert(ei == NULL || ei == &func->e.ei);
                ctx->ei = &func->e.ei;
        }
#if defined(TOYWASM_USE_LOCALS_CACHE)
        ctx->current_locals = frame_locals(ctx, frame);
#endif
}

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
        assert(funcidx == FUNCIDX_INVALID || ctx->paramtype == paramtype);
        assert(funcidx == FUNCIDX_INVALID || ctx->localtype == localtype);
        assert(ctx->ei == ei);
        return 0;
}

void
frame_exit(struct exec_context *ctx)
{
        /*
         * Note: it's caller's responsibility to move results
         * on the operand stack if necessary.
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
                ctx->p = pc2ptr(ctx->instance->module, frame->callerpc);
        }
        assert(frame->labelidx <= ctx->labels.lsize);
        ctx->labels.lsize = frame->labelidx;
#if defined(TOYWASM_USE_SEPARATE_LOCALS)
        assert(frame->localidx <= ctx->locals.lsize);
        ctx->locals.lsize = frame->localidx;
#endif
        frame_clear(frame);
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
                STAT_INC(ctx->stats.jump_cache_hit);
                return jump;
        }
#endif
        STAT_INC(ctx->stats.jump_table_search);
        jump = jump_table_lookup(ei, blockpc);
#if defined(TOYWASM_USE_JUMP_CACHE)
        ctx->jump_cache = jump;
#endif
        return jump;
}

static const struct func *
funcinst_func(const struct funcinst *fi)
{
        assert(!fi->is_host);
        struct instance *inst = fi->u.wasm.instance;
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
        ctx->stack.lsize -= nparams;
        ret = finst->u.host.func(ctx, finst->u.host.instance, ft,
                                 &VEC_NEXTELEM(ctx->stack),
                                 &VEC_NEXTELEM(ctx->stack));
        assert(IS_RESTARTABLE(ret) || ctx->restart_type == RESTART_NONE);
        if (ret != 0) {
                if (IS_RESTARTABLE(ret)) {
                        /*
                         * Restore the stack pointer for restarting.
                         *
                         * Note: it's a responsibility of host functions
                         * to keep function arguments on the stack intact
                         * when returning a restartable error.
                         */
                        ctx->stack.lsize += nparams;
                        STAT_INC(ctx->stats.call_restart);
                }
                return ret;
        }
        ctx->stack.lsize += nresults;
        assert(ctx->stack.lsize <= ctx->stack.psize);
        return 0;
}

static int
do_call(struct exec_context *ctx, const struct funcinst *finst)
{
        STAT_INC(ctx->stats.call);
        if (finst->is_host) {
                STAT_INC(ctx->stats.host_call);
                return do_host_call(ctx, finst);
        } else {
                return do_wasm_call(ctx, finst);
        }
}

#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
static int
do_return_call(struct exec_context *ctx, const struct funcinst *finst)
{
        const struct funcframe *frame = &VEC_LASTELEM(ctx->frames);
        uint32_t height = frame->height;
        frame_exit(ctx);

        const struct functype *ft = funcinst_functype(finst);
        uint32_t arity = resulttype_cellsize(&ft->parameter);
        rewind_stack(ctx, height, arity);

        STAT_INC(ctx->stats.tail_call);
        if (finst->is_host) {
                STAT_INC(ctx->stats.host_tail_call);
        }
        return do_call(ctx, finst);
}
#endif /* defined(TOYWASM_ENABLE_WASM_TAILCALL) */

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
                STAT_INC(ctx->stats.jump_loop);
                const int64_t blocktype = read_leb_s33_nocheck(&p);
                get_arity_for_blocktype(m, blocktype, &param_arity, &arity);
                ctx->p = blockp;
                arity = param_arity;
        }
        *param_arityp = param_arity;
        *arityp = arity;
        return false;
}

static bool
cached_block_exit(struct exec_context *ctx, uint32_t blockpc, bool goto_else,
                  uint32_t *param_arityp, uint32_t *arityp)
{
        uint32_t param_arity;
        uint32_t arity;
#if TOYWASM_JUMP_CACHE2_SIZE > 0
        const struct jump_cache *cache;
        if ((cache = jump_cache2_lookup(ctx, blockpc, goto_else)) != NULL) {
                STAT_INC(ctx->stats.jump_cache2_hit);
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
                STAT_INC(ctx->stats.branch_goto_else);
        } else {
                STAT_INC(ctx->stats.branch);
        }
        if (ctx->labels.lsize - labelidx == frame->labelidx) {
                /*
                 * Exit the function.
                 */
                xlog_trace_insn("do_branch: exiting function");
                frame_exit(ctx);
                height = frame->height;
                arity = frame->nresults;
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
                        STAT_INC(ctx->stats.interrupt_user);
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
                STAT_INC(ctx->stats.interrupt_usched);
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
                STAT_INC(ctx->stats.interrupt_debug);
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
                                        STAT_INC(ctx->stats.tail_call_restart);
                                }
                                return ret;
                        }
                        break;
#endif /* defined(TOYWASM_ENABLE_WASM_TAILCALL) */
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
                if (ctx->frames.lsize == 0) {
                        break;
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
                                        STAT_INC(ctx->stats.exec_loop_restart);
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

int
memory_init(struct exec_context *ectx, uint32_t memidx, uint32_t dataidx,
            uint32_t d, uint32_t s, uint32_t n)
{
        struct instance *inst = ectx->instance;
        const struct module *m = inst->module;
        assert(memidx < m->nimportedmems + m->nmems);
        assert(dataidx < m->ndatas);
        int ret;
        bool dropped = bitmap_test(&inst->data_dropped, dataidx);
        const struct data *data = &m->datas[dataidx];
        if ((dropped && !(s == 0 && n == 0)) || s > data->init_size ||
            n > data->init_size - s) {
                ret = trap_with_id(
                        ectx, TRAP_OUT_OF_BOUNDS_DATA_ACCESS,
                        "out of bounds data access: dataidx %" PRIu32
                        ", dropped %u, init_size %" PRIu32 ", s %" PRIu32
                        ", n %" PRIu32,
                        dataidx, dropped, data->init_size, s, n);
                goto fail;
        }
        void *p;
        ret = memory_getptr(ectx, memidx, d, 0, n, &p);
        if (ret != 0) {
                goto fail;
        }
        memcpy(p, &data->init[s], n);
        ret = 0;
fail:
        return ret;
}

int
table_access(struct exec_context *ectx, uint32_t tableidx, uint32_t offset,
             uint32_t n)
{
        struct instance *inst = ectx->instance;
        const struct module *m = inst->module;
        assert(tableidx < m->nimportedtables + m->ntables);
        struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
        if (offset > t->size || n > t->size - offset) {
                return trap_with_id(
                        ectx, TRAP_OUT_OF_BOUNDS_TABLE_ACCESS,
                        "out of bounds table access: table %" PRIu32
                        ", size %" PRIu32 ", offset %" PRIu32 ", n %" PRIu32,
                        tableidx, t->size, offset, n);
        }
        return 0;
}

int
table_init(struct exec_context *ectx, uint32_t tableidx, uint32_t elemidx,
           uint32_t d, uint32_t s, uint32_t n)
{
        struct instance *inst = ectx->instance;
        const struct module *m = inst->module;
        assert(tableidx < m->nimportedtables + m->ntables);
        assert(elemidx < m->nelems);
        int ret;
        bool dropped = bitmap_test(&inst->elem_dropped, elemidx);
        struct element *elem = &m->elems[elemidx];
        if ((dropped && !(s == 0 && n == 0)) || s > elem->init_size ||
            n > elem->init_size - s) {
                ret = trap_with_id(ectx, TRAP_OUT_OF_BOUNDS_ELEMENT_ACCESS,
                                   "out of bounds element access: dataidx "
                                   "%" PRIu32
                                   ", dropped %u, init_size %" PRIu32
                                   ", s %" PRIu32 ", n %" PRIu32,
                                   elemidx, dropped, elem->init_size, s, n);
                goto fail;
        }
        ret = table_access(ectx, tableidx, d, n);
        if (ret != 0) {
                goto fail;
        }
        struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
        assert(t->type->et == elem->type);
        uint32_t csz = valtype_cellsize(t->type->et);
        uint32_t i;
        for (i = 0; i < n; i++) {
                struct val val;
                if (elem->funcs != NULL) {
                        val.u.funcref.func =
                                VEC_ELEM(inst->funcs, elem->funcs[s + i]);
                } else {
                        ret = exec_const_expr(&elem->init_exprs[s + i],
                                              elem->type, &val, ectx);
                        if (ret != 0) {
                                goto fail;
                        }
                }
                val_to_cells(&val, &t->cells[(d + i) * csz], csz);
                xlog_trace("table %" PRIu32 " offset %" PRIu32
                           " initialized to %016" PRIx64,
                           tableidx, d + i, val.u.i64);
        }
        ret = 0;
fail:
        return ret;
}

void
table_set(struct tableinst *tinst, uint32_t elemidx, const struct val *val)
{
        uint32_t csz = valtype_cellsize(tinst->type->et);
        val_to_cells(val, &tinst->cells[elemidx * csz], csz);
}

void
table_get(struct tableinst *tinst, uint32_t elemidx, struct val *val)
{
        uint32_t csz = valtype_cellsize(tinst->type->et);
        val_from_cells(val, &tinst->cells[elemidx * csz], csz);
}

int
table_grow(struct tableinst *t, const struct val *val, uint32_t n)
{
        if (UINT32_MAX - t->size < n || t->size + n > t->type->lim.max) {
                return (uint32_t)-1;
        }

        uint32_t newsize = t->size + n;
        uint32_t csz = valtype_cellsize(t->type->et);
        uint32_t newncells = newsize * csz;
        int ret;
        if (newncells / csz != newsize) {
                ret = EOVERFLOW;
        } else {
                ret = ARRAY_RESIZE(t->cells, newncells);
        }
        if (ret != 0) {
                return (uint32_t)-1;
        }

        uint32_t i;
        for (i = t->size; i < newsize; i++) {
                val_to_cells(val, &t->cells[i * csz], csz);
        }
        uint32_t oldsize = t->size;
        t->size = newsize;
        return oldsize;
}

void
global_set(struct globalinst *ginst, const struct val *val)
{
        ginst->val = *val;
}

void
global_get(struct globalinst *ginst, struct val *val)
{
        *val = ginst->val;
}

void
exec_context_init(struct exec_context *ctx, struct instance *inst)
{
        memset(ctx, 0, sizeof(*ctx));
        ctx->instance = inst;
        report_init(&ctx->report0);
        ctx->report = &ctx->report0;
        ctx->restart_type = RESTART_NONE;
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
        report_clear(&ctx->report0);
        ctx->report = NULL;
}

#define VEC_PRINT_USAGE(name, vec)                                            \
        nbio_printf("%s %" PRIu32 " (%zu bytes)\n", (name), (vec)->psize,     \
                    (vec)->psize * sizeof(*(vec)->p));

#define STAT_PRINT(name)                                                      \
        nbio_printf("%23s %12" PRIu64 "\n", #name, ctx->stats.name);

void
exec_context_print_stats(struct exec_context *ctx)
{
        printf("=== execution statistics ===\n");
        VEC_PRINT_USAGE("operand stack", &ctx->stack);
#if defined(TOYWASM_USE_SEPARATE_LOCALS)
        VEC_PRINT_USAGE("locals", &ctx->locals);
#endif
        VEC_PRINT_USAGE("labels", &ctx->labels);
        VEC_PRINT_USAGE("frames", &ctx->frames);

        STAT_PRINT(call);
        STAT_PRINT(host_call);
        STAT_PRINT(tail_call);
        STAT_PRINT(host_tail_call);
        STAT_PRINT(branch);
        STAT_PRINT(branch_goto_else);
        STAT_PRINT(jump_cache2_hit);
        STAT_PRINT(jump_cache_hit);
        STAT_PRINT(jump_table_search);
        STAT_PRINT(jump_loop);
        STAT_PRINT(type_annotation_lookup1);
        STAT_PRINT(type_annotation_lookup2);
        STAT_PRINT(type_annotation_lookup3);
        STAT_PRINT(interrupt_exit);
        STAT_PRINT(interrupt_suspend);
        STAT_PRINT(interrupt_usched);
        STAT_PRINT(interrupt_user);
        STAT_PRINT(interrupt_debug);
        STAT_PRINT(exec_loop_restart);
        STAT_PRINT(call_restart);
        STAT_PRINT(tail_call_restart);
        STAT_PRINT(atomic_wait_restart);
}

uint32_t
find_type_annotation(struct exec_context *ctx, const uint8_t *p)
{
#if defined(TOYWASM_USE_SMALL_CELLS)
        const struct expr_exec_info *ei = ctx->ei;
        const struct type_annotations *an = &ei->type_annotations;
        assert(an->default_size > 0);
        if (an->ntypes == 0) {
                STAT_INC(ctx->stats.type_annotation_lookup1);
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
                STAT_INC(ctx->stats.type_annotation_lookup2);
                return an->default_size;
        }
        assert(an->types[i - 1].size > 0);
        STAT_INC(ctx->stats.type_annotation_lookup3);
        return an->types[i - 1].size;
#else
        return 1;
#endif
}

static void
memory_lock(struct meminst *mi) NO_THREAD_SAFETY_ANALYSIS
{
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        struct shared_meminst *shared = mi->shared;
        if (shared != NULL) {
                toywasm_mutex_lock(&shared->lock);
        }
#endif
}

static void
memory_unlock(struct meminst *mi) NO_THREAD_SAFETY_ANALYSIS
{
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        struct shared_meminst *shared = mi->shared;
        if (shared != NULL) {
                toywasm_mutex_unlock(&shared->lock);
        }
#endif
}

uint32_t
memory_grow(struct exec_context *ctx, uint32_t memidx, uint32_t sz)
{
        struct instance *inst = ctx->instance;
        const struct module *m = inst->module;
        assert(memidx < m->nimportedmems + m->nmems);
        struct meminst *mi = VEC_ELEM(inst->mems, memidx);
        const struct memtype *mt = mi->type;
        const struct limits *lim = &mt->lim;
        memory_lock(mi);
        uint32_t orig_size;
#if defined(TOYWASM_ENABLE_WASM_THREADS)
retry:
#endif
        orig_size = mi->size_in_pages;
        uint32_t new_size = orig_size + sz;
        assert(lim->max <= WASM_MAX_PAGES);
        if (new_size > lim->max) {
                memory_unlock(mi);
                return (uint32_t)-1; /* fail */
        }
        xlog_trace("memory grow %" PRIu32 " -> %" PRIu32, mi->size_in_pages,
                   new_size);
        bool do_realloc = new_size != orig_size;
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        const bool shared = mi->shared != NULL;
        if (shared) {
#if defined(TOYWASM_PREALLOC_SHARED_MEMORY)
                do_realloc = false;
#endif
        } else
#endif /* defined(TOYWASM_ENABLE_WASM_THREADS) */
                if (new_size < 4) {
                        /*
                         * Note: for small non-shared memories, we defer the
                         * actual reallocation to memory_getptr2. (mainly to
                         * allow sub-page usage.)
                         */
                        do_realloc = false;
                }

        if (do_realloc) {
#if defined(TOYWASM_ENABLE_WASM_THREADS)
                struct cluster *c = ctx->cluster;
                if (shared && c != NULL) {
                        /*
                         * suspend all other threads to ensure that no one is
                         * accessing the shared memory.
                         *
                         * REVISIT: doing this on every memory.grow is a bit
                         * expensive.
                         * maybe we can mitigate it by alloctating a bit more
                         * than requested.
                         */
                        memory_unlock(mi);
                        suspend_threads(c);
                        memory_lock(mi);
                        if (mi->size_in_pages != orig_size) {
                                goto retry;
                        }
                        assert((mi->allocated % WASM_PAGE_SIZE) == 0);
                }
#endif /* defined(TOYWASM_ENABLE_WASM_THREADS) */
                assert(new_size > mi->allocated / WASM_PAGE_SIZE);
                int ret;
                ret = resize_array((void **)&mi->data, WASM_PAGE_SIZE,
                                   new_size);
                if (ret == 0) {
                        /*
                         * Note: overflow check is already done in
                         * resize_array
                         */
                        size_t new_allocated =
                                (size_t)new_size * WASM_PAGE_SIZE;
                        assert(new_allocated > mi->allocated);
                        memset(mi->data + mi->allocated, 0,
                               new_allocated - mi->allocated);
                        mi->allocated = new_allocated;
                }
#if defined(TOYWASM_ENABLE_WASM_THREADS)
                if (shared && c != NULL) {
                        resume_threads(c);
                }
#endif /* defined(TOYWASM_ENABLE_WASM_THREADS) */
                if (ret != 0) {
                        memory_unlock(mi);
                        xlog_trace("%s: realloc failed", __func__);
                        return (uint32_t)-1; /* fail */
                }
        }
        mi->size_in_pages = new_size;
        memory_unlock(mi);
        return orig_size; /* success */
}

#if defined(TOYWASM_ENABLE_WASM_THREADS)
int
memory_notify(struct exec_context *ctx, uint32_t memidx, uint32_t addr,
              uint32_t offset, uint32_t count, uint32_t *nwokenp)
{
        struct instance *inst = ctx->instance;
        const struct module *m = inst->module;
        assert(memidx < m->nimportedmems + m->nmems);
        struct meminst *mi = VEC_ELEM(inst->mems, memidx);
        struct shared_meminst *shared = mi->shared;
        struct toywasm_mutex *lock;
        void *p;
        int ret;
#if defined(__GNUC__) && !defined(__clang__)
        lock = NULL;
#endif
        ret = memory_atomic_getptr(ctx, memidx, addr, offset, 4, &p, &lock);
        if (ret != 0) {
                return ret;
        }
        assert((lock == NULL) == (shared == NULL));
        uint32_t nwoken;
        if (shared == NULL) {
                /* non-shared memory. we never have waiters. */
                nwoken = 0;
        } else {
                nwoken = atomics_notify(&shared->tab, addr + offset, count);
        }
        memory_atomic_unlock(lock);
        *nwokenp = nwoken;
        return 0;
}

int
memory_wait(struct exec_context *ctx, uint32_t memidx, uint32_t addr,
            uint32_t offset, uint64_t expected, uint32_t *resultp,
            int64_t timeout_ns, bool is64)
{
        struct instance *inst = ctx->instance;
        const struct module *m = inst->module;
        assert(memidx < m->nimportedmems + m->nmems);
        struct meminst *mi = VEC_ELEM(inst->mems, memidx);
        struct shared_meminst *shared = mi->shared;
        if (shared == NULL) {
                /* non-shared memory. */
                return trap_with_id(ctx, TRAP_ATOMIC_WAIT_ON_NON_SHARED_MEMORY,
                                    "wait on non-shared memory");
        }
        struct toywasm_mutex *lock = NULL;
        int ret;

        /*
         * Note: it's important to always consume restart_abstimeout.
         */
        const struct timespec *abstimeout = NULL;
        assert(ctx->restart_type == RESTART_NONE ||
               ctx->restart_type == RESTART_TIMER);
        if (ctx->restart_type == RESTART_TIMER) {
                abstimeout = &ctx->restart_u.timer.abstimeout;
                ctx->restart_type = RESTART_NONE;
        } else if (timeout_ns >= 0) {
                ret = abstime_from_reltime_ns(CLOCK_REALTIME,
                                              &ctx->restart_u.timer.abstimeout,
                                              timeout_ns);
                if (ret != 0) {
                        goto fail;
                }
                abstimeout = &ctx->restart_u.timer.abstimeout;
        }
        assert(ctx->restart_type == RESTART_NONE);
        const uint32_t sz = is64 ? 8 : 4;
        void *p;
        ret = memory_atomic_getptr(ctx, memidx, addr, offset, sz, &p, &lock);
        if (ret != 0) {
                return ret;
        }
        assert((lock == NULL) == (shared == NULL));
retry:;
        uint64_t prev;
        if (is64) {
                prev = *(_Atomic uint64_t *)p;
        } else {
                prev = *(_Atomic uint32_t *)p;
        }
        xlog_trace("%s: addr=0x%" PRIx32 " offset=0x%" PRIx32
                   " actual=%" PRIu64 " expected %" PRIu64,
                   __func__, addr, offset, prev, expected);
        if (prev != expected) {
                *resultp = 1; /* not equal */
        } else {
                ret = check_interrupt(ctx);
                if (ret != 0) {
                        goto fail;
                }
                struct timespec next_abstimeout;
                const int interval_ms = check_interrupt_interval_ms(ctx);
                ret = abstime_from_reltime_ms(CLOCK_REALTIME, &next_abstimeout,
                                              interval_ms);
                if (ret != 0) {
                        goto fail;
                }
                const struct timespec *tv;
                if (abstimeout != NULL &&
                    timespec_cmp(&next_abstimeout, abstimeout) >= 0) {
                        tv = abstimeout;
                        xlog_trace("%s: abs %ju.%09lu\n", __func__,
                                   (uintmax_t)tv->tv_sec, tv->tv_nsec);
                } else {
                        tv = &next_abstimeout;
                        xlog_trace("%s: next %ju.%09lu\n", __func__,
                                   (uintmax_t)tv->tv_sec, tv->tv_nsec);
                }
                ret = atomics_wait(&shared->tab, addr + offset, tv);
                if (ret == 0) {
                        *resultp = 0; /* ok */
                } else if (ret == ETIMEDOUT) {
                        if (tv != abstimeout) {
                                goto retry;
                        }
                        *resultp = 2; /* timed out */
                } else {
                        goto fail;
                }
        }
        ret = 0;
fail:
        if (IS_RESTARTABLE(ret)) {
                if (abstimeout != NULL) {
                        assert(abstimeout == &ctx->restart_u.timer.abstimeout);
                        ctx->restart_type = RESTART_TIMER;
                }
                STAT_INC(ctx->stats.atomic_wait_restart);
        }
        memory_atomic_unlock(lock);
        if (ret == 0) {
                xlog_trace("%s: returning %d result %d", __func__, ret,
                           *resultp);
        } else {
                xlog_trace("%s: returning %d", __func__, ret);
        }
        return ret;
}
#endif

/*
 * invoke: call a function.
 *
 * Note: the "finst" here can be a host function.
 */
int
invoke(struct funcinst *finst, const struct resulttype *paramtype,
       const struct resulttype *resulttype, struct exec_context *ctx)
{
        const struct functype *ft = funcinst_functype(finst);

        /*
         * Optional type check.
         */
        assert((paramtype == NULL) == (resulttype == NULL));
        if (paramtype != NULL) {
                if (compare_resulttype(paramtype, &ft->parameter) != 0 ||
                    compare_resulttype(resulttype, &ft->result) != 0) {
                        return EINVAL;
                }
        }

        /* Sanity check */
        assert(ctx->stack.lsize >= resulttype_cellsize(&ft->parameter));

        /*
         * Set up the context as if it was a restart of a "call" instruction.
         */

        ctx->event_u.call.func = finst;
        ctx->event = EXEC_EVENT_CALL;

        /*
         * and then "restart" the context execution.
         */
        return ETOYWASMRESTART;
}

void
data_drop(struct exec_context *ectx, uint32_t dataidx)
{
        struct instance *inst = ectx->instance;
        const struct module *m = inst->module;
        assert(dataidx < m->ndatas);
        bitmap_set(&inst->data_dropped, dataidx);
}

void
elem_drop(struct exec_context *ectx, uint32_t elemidx)
{
        struct instance *inst = ectx->instance;
        const struct module *m = inst->module;
        assert(elemidx < m->nelems);
        bitmap_set(&inst->elem_dropped, elemidx);
}

void
print_locals(const struct exec_context *ctx, const struct funcframe *fp)
{
        const struct instance *inst = fp->instance;
        const struct funcinst *finst = VEC_ELEM(inst->funcs, fp->funcidx);
        const struct functype *ft = funcinst_functype(finst);
        const struct func *func = funcinst_func(finst);
        const struct resulttype *rt = &ft->parameter;
        const struct localtype *lt = &func->localtype;
        const struct cell *locals = frame_locals(ctx, fp);

        uint32_t i;
        for (i = 0; i < rt->ntypes; i++) {
                uint32_t csz;
                uint32_t cidx = resulttype_cellidx(rt, i, &csz);
                struct val val;
                val_from_cells(&val, locals + cidx, csz);
                switch (csz) {
                case 1:
                        printf("param [%" PRIu32 "] = %08" PRIx32 "\n", i,
                               val.u.i32);
                        break;
                case 2:
                        printf("param [%" PRIu32 "] = %016" PRIx64 "\n", i,
                               val.u.i64);
                        break;
                default:
                        printf("param [%" PRIu32 "] = unknown size\n", i);
                        break;
                }
        }
        uint32_t localstart = rt->ntypes;
        uint32_t localstartcidx = resulttype_cellsize(rt);
        for (i = 0; i < lt->nlocals; i++) {
                uint32_t csz;
                uint32_t cidx = localtype_cellidx(lt, i, &csz);
                struct val val;
                val_from_cells(&val, locals + localstartcidx + cidx, csz);
                switch (csz) {
                case 1:
                        printf("local [%" PRIu32 "] = %08" PRIx32 "\n",
                               localstart + i, val.u.i32);
                        break;
                case 2:
                        printf("local [%" PRIu32 "] = %016" PRIx64 "\n",
                               localstart + i, val.u.i64);
                        break;
                default:
                        printf("local [%" PRIu32 "] = unknown size\n",
                               localstart + i);
                        break;
                }
        }
}

void
print_trace(const struct exec_context *ctx)
{
        const struct funcframe *fp;
        uint32_t i;
        VEC_FOREACH_IDX(i, fp, ctx->frames) {
                const struct instance *inst = fp->instance;
                const struct funcinst *finst =
                        VEC_ELEM(inst->funcs, fp->funcidx);
                const struct func *func = funcinst_func(finst);
                /*
                 * XXX funcpc here is the address of the first expr.
                 *
                 * it seems more common to use the address of the size LEB.
                 * at least it's the convention used by wasm-objdump etc.
                 * our funcpc is usually a few bytes ahead. (the size LEB
                 * and the following definition of locals)
                 */
                uint32_t funcpc = ptr2pc(inst->module, func->e.start);
                /* no callerpc for the first frame */
                if (i == 0) {
                        printf("frame[%3" PRIu32 "] funcpc %06" PRIx32 "\n", i,
                               funcpc);
                } else {
                        printf("frame[%3" PRIu32 "] funcpc %06" PRIx32
                               " callerpc %06" PRIx32 "\n",
                               i, funcpc, fp->callerpc);
                }
                print_locals(ctx, fp);
        }
}

void
print_memory(const struct exec_context *ctx, const struct instance *inst,
             uint32_t memidx, uint32_t addr, uint32_t count)
{
        printf("==== print_memory start ====\n");
        const struct module *m = inst->module;
        if (memidx >= m->nmems + m->nimportedmems) {
                printf("%s: out of range memidx\n", __func__);
                goto fail;
        }
        const struct meminst *mi = VEC_ELEM(inst->mems, memidx);
        if (addr >= mi->allocated) {
                printf("%s: not allocated yet\n", __func__);
                goto fail;
        }
        if (mi->allocated - addr < count) {
                printf("%s: dump truncated\n", __func__);
                count = mi->allocated - addr;
        }
        const uint8_t *p = mi->data + addr;
        const char *sep = "";
        uint32_t i;
        for (i = 0; i < count; i++) {
                if ((i % 16) == 0) {
                        printf("%04" PRIx32 ":%08" PRIx32 ":", memidx,
                               addr + i);
                }
                printf("%s%02x", sep, p[i]);
                sep = " ";
        }
        printf("\n");
fail:
        printf("==== print_memory end ====\n");
}

void
print_pc(const struct exec_context *ctx)
{
        /*
         * XXX ctx->p is usually not up to date with TOYWASM_USE_TAILCALL.
         * XXX ctx->p usually points to the middle of opcode.
         */
        printf("PC %06" PRIx32 "\n", ptr2pc(ctx->instance->module, ctx->p));
}
