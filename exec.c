
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "exec.h"
#include "expr.h"
#include "insn.h"
#include "leb128.h"
#include "type.h"
#include "util.h"
#include "xlog.h"

int
vtrap(struct exec_context *ctx, enum trapid id, const char *fmt, va_list ap)
{
        int ret;
        ctx->trapped = true;
        ctx->trapid = id;
        free(ctx->trapmsg);
        ctx->trapmsg = NULL;
        ret = vasprintf(&ctx->trapmsg, fmt, ap);
        if (ret < 0) {
                xlog_error("failed to format trap message with %d", errno);
        }
        xlog_error("TRAP: %s", ctx->trapmsg);
        return EFAULT;
}

int
trap(struct exec_context *ctx, const char *fmt, ...)
{
        int ret;
        va_list ap;
        va_start(ap, fmt);
        ret = vtrap(ctx, TRAP_MISC, fmt, ap);
        va_end(ap);
        return ret;
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
memory_getptr(struct exec_context *ctx, uint32_t memidx, uint32_t ptr,
              uint32_t offset, uint32_t size, void **pp)
{
        struct instance *inst = ctx->instance;
        struct meminst *meminst = VEC_ELEM(inst->mems, memidx);
        if (offset > UINT32_MAX - ptr) {
                /*
                 * i failed to find this in the spec.
                 * but some of spec tests seem to test this.
                 */
                goto do_trap;
        }
        uint32_t ea = ptr + offset;
        if (size > UINT32_MAX - ea) {
                goto do_trap;
        }
        uint32_t need = ea + size;
        xlog_trace("memory access: at %04" PRIx32 " %08" PRIx32
                   ", size %" PRIu32 ", meminst size %" PRIu32,
                   memidx, ptr, size, meminst->size_in_pages);
        uint32_t need_in_pages =
                ((uint64_t)need + WASM_PAGE_SIZE - 1) / WASM_PAGE_SIZE;
        if (need_in_pages > meminst->size_in_pages) {
do_trap:
                return trap_with_id(
                        ctx, TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS,
                        "invalid memory access at %04" PRIx32 " %08" PRIx32
                        " + %08" PRIx32 ", size %" PRIu32
                        ", meminst size %" PRIu32,
                        memidx, ptr, offset, size, meminst->size_in_pages);
        }
        /* REVISIT thread */
        if (need > meminst->allocated) {
                int ret = resize_array((void **)&meminst->data, 1, need);
                if (ret != 0) {
                        return ret;
                }
                xlog_trace("extend memory %" PRIu32 " from %" PRIu32
                           " to %" PRIu32,
                           memidx, meminst->allocated, need);
                memset(meminst->data + meminst->allocated, 0,
                       need - meminst->allocated);
                meminst->allocated = need;
        }
        *pp = meminst->data + ea;
        return 0;
}

void
frame_clear(struct funcframe *frame)
{
}

int
stack_prealloc(struct exec_context *ctx, uint32_t count)
{
        uint32_t needed = ctx->stack.lsize + count;
        if (needed >= MAX_STACKVALS) {
                if (needed >= MAX_STACKVALS) {
                        return trap_with_id(
                                ctx, TRAP_TOO_MANY_STACKVALS,
                                "too many values on the operand stack");
                }
        }
        return VEC_PREALLOC(ctx->stack, count);
}

/*
 * https://webassembly.github.io/spec/core/exec/instructions.html#function-calls
 * https://webassembly.github.io/spec/core/exec/runtime.html#default-val
 */

int
frame_enter(struct exec_context *ctx, struct instance *inst,
            const struct expr_exec_info *ei, uint32_t func_nlocals,
            uint32_t nparams, uint32_t nresults, const struct val *params)
{
        /*
         * Note: params can be in ctx->stack.
         * Be careful when resizing the later.
         */

        struct funcframe *frame;
        int ret;

        if (ctx->frames.lsize == MAX_FRAMES) {
                return trap_with_id(ctx, TRAP_TOO_MANY_FRAMES,
                                    "too many frames");
        }
        ret = VEC_PREALLOC(ctx->frames, 1);
        if (ret != 0) {
                return ret;
        }
        const uint32_t nlocals = nparams + func_nlocals;
        frame = &VEC_NEXTELEM(ctx->frames);
        frame->ei = ei;
#if !defined(NDEBUG)
        frame->nlocals = nlocals;
#endif
        frame->nresults = nresults;
        frame->instance = inst;
        frame->labelidx = ctx->labels.lsize;
        frame->localidx = ctx->locals.lsize;
        ret = VEC_PREALLOC(ctx->locals, nlocals);
        if (ret != 0) {
                return ret;
        }
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
        uint32_t i;
        for (i = 0; i < nparams; i++) {
                VEC_ELEM(ctx->locals, frame->localidx + i) = params[i];
        }
        for (; i < nlocals; i++) {
                memset(&VEC_ELEM(ctx->locals, frame->localidx + i), 0,
                       sizeof(*ctx->locals.p));
        }

        xlog_trace("frame enter: maxlabels %u maxvals %u", ei->maxlabels,
                   ei->maxvals);
        for (i = 0; i < nlocals; i++) {
                if (i == nparams) {
                        xlog_trace("-- ^-params v-locals");
                }
                xlog_trace("local [%" PRIu32 "] %016" PRIx64, i,
                           VEC_ELEM(ctx->locals, frame->localidx + i).u.i64);
        }

        /*
         * As we've copied "params" above, now it's safe to resize stack.
         */
        ret = stack_prealloc(ctx, ei->maxvals);
        if (ret != 0) {
                return ret;
        }

        /*
         * commit changes.
         */
        ctx->frames.lsize++;
        assert(ctx->locals.lsize + nlocals <= ctx->locals.psize);
        ctx->locals.lsize += nlocals;
        ctx->instance = inst;
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
        if (ctx->frames.lsize > 0) {
                ctx->instance = VEC_LASTELEM(ctx->frames).instance;
                ctx->p = pc2ptr(ctx->instance->module, frame->callerpc);
        }
        assert(frame->labelidx <= ctx->labels.lsize);
        assert(frame->localidx <= ctx->locals.lsize);
        ctx->labels.lsize = frame->labelidx;
        ctx->locals.lsize = frame->localidx;
        frame_clear(frame);
}

static const struct jump *
jump_lookup(const struct expr_exec_info *ei, uint32_t blockpc)
{
        /* TODO binary search */
        uint32_t i;
        for (i = 0; i < ei->njumps; i++) {
                const struct jump *jump = &ei->jumps[i];
                if (jump->pc == blockpc) {
                        return jump;
                }
        }
        assert(false);
}

static int
do_wasm_call(struct exec_context *ctx, const struct funcinst *finst)
{
        int ret;
        const struct functype *type = funcinst_functype(finst);
        struct instance *callee_inst = finst->u.wasm.instance;
        const struct module *callee = callee_inst->module;
        uint32_t funcidx = finst->u.wasm.funcidx;
        assert(funcidx < callee->nimportedfuncs + callee->nfuncs);
        const struct func *func =
                &callee->funcs[funcidx - callee->nimportedfuncs];
        uint32_t nparams = type->parameter.ntypes;
        uint32_t nresults = type->result.ntypes;
        assert(ctx->stack.lsize >= nparams);
        ctx->stack.lsize -= nparams;
        ret = frame_enter(ctx, callee_inst, &func->e.ei, func->nlocals,
                          nparams, nresults, &VEC_NEXTELEM(ctx->stack));
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
        uint32_t nparams = ft->parameter.ntypes;
        uint32_t nresults = ft->result.ntypes;
        int ret;
        assert(ctx->stack.lsize >= nparams);
        if (nresults > nparams) {
                ret = stack_prealloc(ctx, nresults - nparams);
                if (ret != 0) {
                        return ret;
                }
        }
        ctx->stack.lsize -= nparams;
        ret = finst->u.host.func(ctx, ft, &VEC_NEXTELEM(ctx->stack),
                                 &VEC_NEXTELEM(ctx->stack));
        if (ret != 0) {
                return ret;
        }
        ctx->stack.lsize += nresults;
        assert(ctx->stack.lsize <= ctx->stack.psize);
        return 0;
}

static int
do_call(struct exec_context *ctx, const struct funcinst *finst)
{
        if (finst->is_host) {
                return do_host_call(ctx, finst);
        } else {
                return do_wasm_call(ctx, finst);
        }
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
        if (ctx->labels.lsize - labelidx == frame->labelidx) {
                /* exit the function */
                xlog_trace("do_branch: exititng function");
                frame_exit(ctx);
                height = frame->height;
                arity = frame->nresults;
        } else {
                const struct label *l = &VEC_ELEM(
                        ctx->labels, ctx->labels.lsize - labelidx - 1);
                uint32_t blockpc = l->pc;

                /*
                 * do a jump. (w/ jump table)
                 */
                const struct expr_exec_info *ei = frame->ei;
                if (ei->jumps != NULL) {
                        xlog_trace("jump w/ table");
                        bool stay_in_block = false;
                        const struct jump *jump;
                        jump = jump_lookup(ei, blockpc + goto_else);
                        if (jump->targetpc == 0) {
                                assert(goto_else);
                                jump = jump_lookup(ei, blockpc);
                        } else if (goto_else) {
                                stay_in_block = true;
                        }
                        assert(jump->targetpc != 0);
                        ctx->p = pc2ptr(ctx->instance->module, jump->targetpc);
                        if (stay_in_block) {
                                xlog_trace("jump inside a block");
                                return;
                        }
                }

                /*
                 * exit from the block.
                 *
                 * parse the block op to see
                 * - if it's a "loop"
                 * - otherwise, blocktype
                 */
                struct module *m = ctx->instance->module;
                const uint8_t *blockp = pc2ptr(m, blockpc);
                const uint8_t *p = blockp;
                uint8_t op = *p;
                assert(op == FRAME_OP_LOOP || op == FRAME_OP_IF ||
                       op == FRAME_OP_BLOCK);
                uint32_t param_arity;
                int64_t blocktype;
                int ret;
                p++;
                ret = read_leb_s(&p, NULL, 33, &blocktype);
                assert(ret == 0);
                /*
                 * do a jump. (w/o jump table)
                 */
                if (ei->jumps == NULL) {
                        xlog_trace("jump w/o table");
                        if (op == FRAME_OP_LOOP) {
                                ctx->p = blockp;
                        } else {
                                /*
                                 * The only way to find out the jump target
                                 * is to parse every instructions.
                                 * This is expensive.
                                 */
                                bool stay_in_block = skip_expr(&p, goto_else);
                                ctx->p = p;
                                if (stay_in_block) {
                                        return;
                                }
                        }
                }
                ret = get_arity_for_blocktype(m, blocktype, &param_arity,
                                              &arity);
                if (op == FRAME_OP_LOOP) {
                        arity = param_arity;
                }
                assert(ret == 0);
                ctx->labels.lsize -= labelidx + 1;
                /*
                 * Note: The spec says to pop the values before
                 * pushing the label, which we don't do.
                 * Instead, we adjust the height accordingly here.
                 */
                assert(l->height >= param_arity);
                height = l->height - param_arity;
        }

        /*
         * rewind the operand stack.
         * move the return values.
         */
        assert(height <= ctx->stack.lsize);
        assert(arity <= ctx->stack.lsize);
        memmove(&VEC_ELEM(ctx->stack, height),
                &VEC_ELEM(ctx->stack, ctx->stack.lsize - arity),
                arity * sizeof(*ctx->stack.p));
        ctx->stack.lsize = height + arity;
}

int
exec_expr(const struct expr *expr, uint32_t nlocals,
          const enum valtype *locals, const struct resulttype *parametertype,
          const struct resulttype *resulttype, const struct val *params,
          struct val *results, struct exec_context *ctx)
{
        uint32_t nstackused_saved = ctx->stack.lsize;
        int ret;

        assert(ctx->instance != NULL);
        assert(ctx->instance->module != NULL);

        ret = frame_enter(ctx, ctx->instance, &expr->ei, nlocals,
                          parametertype->ntypes, resulttype->ntypes, params);
        if (ret != 0) {
                return ret;
        }
        struct context common_ctx;
        memset(&common_ctx, 0, sizeof(common_ctx));
        common_ctx.exec = ctx;
        ctx->p = expr->start;
        while (true) {
                uint32_t pc = ptr2pc(ctx->instance->module, ctx->p);
                uint32_t op = *ctx->p++;
                const struct instruction_desc *desc = &instructions[op];
                if (desc->next_table != NULL) {
                        op = *ctx->p++;
                        desc = &desc->next_table[op];
                }
                xlog_trace("exec %06" PRIx32 ": %s", pc, desc->name);
                assert(desc->process != NULL);
#if defined(USE_SEPARATE_EXECUTE)
                ret = desc->execute(&ctx->p, ctx);
#else
                ret = desc->process(&ctx->p, NULL, &common_ctx);
#endif
                if (ret != 0) {
                        if (ctx->trapped) {
                                xlog_trace("got a trap");
                        }
                        return ret;
                }
                if (ctx->call_pending) {
                        assert(ctx->frames.lsize > 0);
                        ret = do_call(ctx, ctx->call_func);
                        if (ret != 0) {
                                return ret;
                        }
                        ctx->call_pending = false;
                }
                if (ctx->branch_pending) {
                        assert(ctx->frames.lsize > 0);
                        do_branch(ctx, ctx->branch_index, ctx->branch_else);
                        ctx->branch_pending = false;
                }
                if (ctx->frames.lsize == 0) {
                        break;
                }
        }
        uint32_t nresults = resulttype->ntypes;
        assert(ctx->stack.lsize == nstackused_saved + nresults);
        memcpy(results, &VEC_ELEM(ctx->stack, ctx->stack.lsize - nresults),
               nresults * sizeof(*results));
        ctx->stack.lsize -= nresults;
        return 0;
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
                        op = *p++;
                        desc = &desc->next_table[op];
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

int
exec_const_expr(const struct expr *expr, enum valtype type, struct val *result,
                struct exec_context *ctx)
{
        assert(ctx->frames.lsize == 0);
        static struct resulttype empty = {
                .ntypes = 0,
                .is_static = true,
        };
        struct resulttype resulttype = {
                .ntypes = 1,
                .types = &type,
                .is_static = true,
        };
        int ret;
        ret = exec_expr(expr, 0, NULL, &empty, &resulttype, NULL, result, ctx);
        if (ret != 0) {
                return ret;
        }
        assert(ctx->frames.lsize == 0);
        return 0;
}

int
memory_init(struct exec_context *ctx, uint32_t dataidx, uint32_t dest,
            uint32_t src, uint32_t n)
{
        struct instance *inst = ctx->instance;
        const struct module *m = inst->module;
        assert(dataidx < m->ndatas);
        const struct data *d = &m->datas[dataidx];
        uint32_t memidx = d->memory;
        int ret;
        if (src + n > d->init_size) {
                ret = trap(ctx, "memory.init out of band source");
                goto fail;
        }
        void *p;
        ret = memory_getptr(ctx, memidx, dest, 0, n, &p);
        if (ret != 0) {
                ret = trap(ctx, "memory.init out of band dest");
                goto fail;
        }
        memcpy(p, d->init + src, n);
        ret = 0;
fail:
        return ret;
}

void
exec_context_init(struct exec_context *ctx, struct instance *inst)
{
        memset(ctx, 0, sizeof(*ctx));
        ctx->instance = inst;
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
        VEC_FREE(ctx->locals);
        free(ctx->trapmsg);
}

uint32_t
memory_grow(struct exec_context *ctx, uint32_t memidx, uint32_t sz)
{
        struct instance *inst = ctx->instance;
        struct module *m = inst->module;
        assert(memidx < m->nimportedmems + m->nmems);
        struct meminst *mi = VEC_ELEM(inst->mems, memidx);
        uint32_t orig_size = mi->size_in_pages;
        uint64_t new_size = (uint64_t)orig_size + sz;
        const struct limits *lim = module_memtype(m, memidx);
        if (new_size > WASM_MAX_PAGES || new_size > lim->max) {
                return (uint32_t)-1; /* fail */
        }
        mi->size_in_pages = new_size;
        return orig_size; /* success */
}
