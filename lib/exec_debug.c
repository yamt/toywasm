#include <assert.h>
#include <inttypes.h>

#include "context.h"
#include "exec.h"
#include "exec_debug.h"
#include "name.h"
#include "nbio.h"
#include "type.h"
#include "xlog.h"

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
        VEC_PRINT_USAGE("restarts", &ctx->restarts);

        STAT_PRINT(call);
        STAT_PRINT(host_call);
#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
        STAT_PRINT(tail_call);
        STAT_PRINT(host_tail_call);
#endif
        STAT_PRINT(branch);
        STAT_PRINT(branch_goto_else);
#if defined(TOYWASM_USE_JUMP_CACHE)
        STAT_PRINT(jump_cache_hit);
#endif
#if TOYWASM_JUMP_CACHE2_SIZE > 0
        STAT_PRINT(jump_cache2_hit);
#endif
        STAT_PRINT(jump_table_search);
        STAT_PRINT(jump_loop);
#if defined(TOYWASM_USE_SMALL_CELLS)
        STAT_PRINT(type_annotation_lookup1);
        STAT_PRINT(type_annotation_lookup2);
        STAT_PRINT(type_annotation_lookup3);
#endif
        STAT_PRINT(interrupt_exit);
        STAT_PRINT(interrupt_suspend);
#if defined(TOYWASM_USE_USER_SCHED)
        STAT_PRINT(interrupt_usched);
#endif
        STAT_PRINT(interrupt_user);
        STAT_PRINT(interrupt_debug);
        STAT_PRINT(exec_loop_restart);
        STAT_PRINT(call_restart);
#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
        STAT_PRINT(tail_call_restart);
#endif
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        STAT_PRINT(atomic_wait_restart);
#endif
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        STAT_PRINT(exception);
#endif
}

static void
print_params(const struct cell *cells, const struct resulttype *rt)
{
        uint32_t i;
        for (i = 0; i < rt->ntypes; i++) {
                uint32_t csz;
                uint32_t cidx = resulttype_cellidx(rt, i, &csz);
                struct val val;
                val_from_cells(&val, cells + cidx, csz);
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

        print_params(locals, rt);
        uint32_t localstart = rt->ntypes;
        uint32_t localstartcidx = resulttype_cellsize(rt);
        uint32_t i;
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
        struct nametable table;
        uint32_t frameidx = ctx->frames.lsize;
        uint32_t restartidx = ctx->restarts.lsize;
        uint32_t bottom = ctx->bottom;
        nametable_init(&table);
        while (true) {
                if (frameidx == bottom) {
                        if (restartidx == 0) {
                                assert(bottom == 0);
                                break;
                        }
                        restartidx--;
                        const struct restart_info *restart =
                                &VEC_ELEM(ctx->restarts, restartidx);
                        assert(restart->restart_type == RESTART_HOSTFUNC);
                        const struct restart_hostfunc *hf =
                                &restart->restart_u.hostfunc;
                        assert(bottom >= hf->saved_bottom);
                        bottom = hf->saved_bottom;
                        const struct funcinst *fi = hf->func;
                        assert(fi->is_host);
                        printf("frame(host) %p %p user1 %08" PRIx32
                               " user2 %08" PRIx32 "\n",
                               (const void *)fi, (const void *)fi->u.host.func,
                               hf->user1, hf->user2);
                        const struct functype *ft = funcinst_functype(fi);
                        const struct resulttype *rt = &ft->parameter;
                        uint32_t stackidx;
                        if (frameidx < ctx->frames.lsize) {
                                const struct funcframe *fp =
                                        &VEC_ELEM(ctx->frames, frameidx);
                                stackidx = fp->height;
                        } else {
                                stackidx = ctx->stack.lsize;
                        }
                        const struct cell *stack = &VEC_ELEM(
                                ctx->stack, stackidx - hf->stack_adj);
                        print_params(stack, rt);
                        continue;
                }
                assert(frameidx > 0);
                frameidx--;
                const struct funcframe *fp = &VEC_ELEM(ctx->frames, frameidx);
                const struct instance *inst = fp->instance;
                const struct module *m = inst->module;
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

                uint32_t funcpc = ptr2pc(m, func->e.start);
                struct name func_name;
                nametable_lookup_func(&table, m, fp->funcidx, &func_name);
                struct name module_name;
                nametable_lookup_module(&table, m, &module_name);
                /* no callerpc for the first frame */
                if (frameidx == 0) {
                        printf("frame[%3" PRIu32 "] funcpc %06" PRIx32
                               " (%.*s:%.*s)\n",
                               frameidx, funcpc, CSTR(&module_name),
                               CSTR(&func_name));
                } else {
                        printf("frame[%3" PRIu32 "] funcpc %06" PRIx32
                               " (%.*s:%.*s)"
                               " callerpc %06" PRIx32 "\n",
                               frameidx, funcpc, CSTR(&module_name),
                               CSTR(&func_name), fp->callerpc);
                }
                print_locals(ctx, fp);
        }
        nametable_clear(&table);
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
