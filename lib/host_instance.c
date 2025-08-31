#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "cconv.h"
#include "exec.h"
#include "host_instance.h"
#include "instance.h"
#include "mem.h"
#include "type.h"
#include "xlog.h"

static void
_dtor(struct mem_context *mctx, struct import_object *im)
{
        struct funcinst *fis = im->dtor_arg;
        if (fis != NULL) {
                uint32_t nfuncs = im->nentries;
                uint32_t i;
                for (i = 0; i < nfuncs; i++) {
                        struct funcinst *fi = &fis[i];
                        if (fi->u.host.type != NULL) {
                                /* discard const */
                                functype_free(mctx, (void *)fi->u.host.type);
                        }
                }
                mem_free(mctx, fis, im->nentries * sizeof(*fis));
        }
}

int
import_object_create_for_host_funcs(struct mem_context *mctx,
                                    const struct host_module *modules,
                                    size_t n, struct host_instance *hi,
                                    struct import_object **impp)
{
        struct import_object *im = NULL;
        struct funcinst *fis = NULL;
        size_t nfuncs;
        size_t i;
        int ret;

        nfuncs = 0;
        for (i = 0; i < n; i++) {
                const struct host_module *hm = &modules[i];
                nfuncs += hm->nfuncs;
        }

        assert(nfuncs > 0);
        ret = import_object_alloc(mctx, nfuncs, &im);
        if (ret != 0) {
                goto fail;
        }
        fis = mem_zalloc(mctx, nfuncs * sizeof(*fis));
        if (fis == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        im->dtor = _dtor;
        im->dtor_arg = fis;
        size_t idx = 0;
        for (i = 0; i < n; i++) {
                const struct host_module *hm = &modules[i];
                size_t j;
                for (j = 0; j < hm->nfuncs; j++) {
                        const struct host_func *func = &hm->funcs[j];
                        struct functype *ft;
                        ret = functype_from_string(mctx, func->type, &ft);
                        if (ret != 0) {
                                xlog_error("failed to parse functype %s with "
                                           "error %d",
                                           func->type, ret);
                                goto fail;
                        }
                        struct funcinst *fi = &fis[idx];
                        fi->is_host = true;
                        fi->u.host.func = func->func;
                        fi->u.host.type = ft;
                        fi->u.host.instance = hi;
                        struct import_object_entry *e = &im->entries[idx];
                        e->module_name = hm->module_name;
                        e->name = &func->name;
                        e->type = EXTERNTYPE_FUNC;
                        e->u.func = fi;
                        idx++;
                }
        }
        assert(idx == nfuncs);
        *impp = im;
        return 0;
fail:
        if (im != NULL) {
                import_object_destroy(mctx, im);
        }
        return ret;
}

void
host_func_dump_params(const struct functype *ft, const struct cell *params)
{
#if defined(TOYWASM_ENABLE_TRACING)
        if (xlog_tracing == 0) {
                return;
        }
        const struct resulttype *rt = &ft->parameter;
        uint32_t i;
        uint32_t cidx = 0;
        for (i = 0; i < rt->ntypes; i++) {
                enum valtype type = rt->types[i];
                uint32_t sz = valtype_cellsize(type);
                struct val val;
                val_from_cells(&val, &params[cidx], sz);
#if defined(TOYWASM_USE_SMALL_CELLS)
                switch (sz) {
                case 1:
                        xlog_trace("param[%" PRIu32 "] = %08" PRIx32, i,
                                   val.u.i32);
                        break;
                case 2:
                        xlog_trace("param[%" PRIu32 "] = %016" PRIx64, i,
                                   val.u.i64);
                        break;
                }
#else
                xlog_trace("param[%" PRIu32 "] = %016" PRIu64, i, val.u.i64);
#endif
                cidx += sz;
        }
#endif /* defined(TOYWASM_ENABLE_TRACING) */
}

/*
 * Trap on unaligned pointers in a host call.
 *
 * Note: This is a relatively new requirement.
 * cf. https://github.com/WebAssembly/WASI/pull/523
 *
 * Open question: Is this appropriate for non-WASI host calls?
 * In general, linear memory alignment doesn't matter in wasm.
 * eg. opcodes like i32.load. If a host function doesn't care
 * the alignment, it can use align=1.
 */
int
host_func_check_align(struct exec_context *ctx, uint32_t wasmaddr,
                      size_t align)
{
        assert(align != 0);
        if ((wasmaddr & (align - 1)) == 0) {
                return 0;
        }
        return trap_with_id(ctx, TRAP_UNALIGNED_MEMORY_ACCESS,
                            "unaligned access to address %" PRIx32
                            " in a host call (expected alignment %" PRIu32 ")",
                            wasmaddr, (uint32_t)align);
}

int
host_func_copyin(struct exec_context *ctx, struct meminst *mem, void *hostaddr,
                 uint32_t wasmaddr, size_t len, size_t align)
{
        void *p;
        int ret;
        ret = host_func_check_align(ctx, wasmaddr, align);
        if (ret != 0) {
                return ret;
        }
        ret = host_func_getptr(ctx, mem, wasmaddr, len, &p);
        if (ret != 0) {
                return ret;
        }
        memcpy(hostaddr, p, len);
        return 0;
}

int
host_func_copyout(struct exec_context *ctx, struct meminst *mem,
                  const void *hostaddr, uint32_t wasmaddr, size_t len,
                  size_t align)
{
        void *p;
        int ret;
        ret = host_func_check_align(ctx, wasmaddr, align);
        if (ret != 0) {
                return ret;
        }
        ret = host_func_getptr(ctx, mem, wasmaddr, len, &p);
        if (ret != 0) {
                return ret;
        }
        memcpy(p, hostaddr, len);
        return 0;
}

int
host_func_getptr(struct exec_context *ctx, struct meminst *mem, uint32_t ptr,
                 uint32_t size, void **pp)
{
        return host_func_getptr2(ctx, mem, ptr, size, pp, NULL);
}

int
host_func_getptr2(struct exec_context *ctx, struct meminst *mem, uint32_t ptr,
                  uint32_t size, void **pp, bool *movedp)
{
        if (mem == NULL) {
                return trap_with_id(
                        ctx, TRAP_MEMORY_NOT_FOUND,
                        "host function invalid memory access at %08" PRIx32
                        ", size %" PRIu32 ", no suitable memory",
                        ptr, size);
        }
        int ret = memory_instance_getptr2(mem, ptr, 0, size, pp, movedp);
        if (ret == ETOYWASMTRAP) {
                return trap_with_id(
                        ctx, TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS,
                        "host function invalid memory access at %08" PRIx32
                        ", size %" PRIu32 ", meminst size %" PRIu32
                        ", pagesize %" PRIu32,
                        ptr, size, mem->size_in_pages,
                        memtype_page_size(mem->type));
        }
        return ret;
}

int
host_func_trap(struct exec_context *ctx, const char *fmt, ...)
{
        int ret;
        va_list ap;
        va_start(ap, fmt);
        ret = vtrap(ctx, TRAP_MISC, fmt, ap);
        va_end(ap);
        return ret;
}

int
schedule_call_from_hostfunc(struct exec_context *ctx,
                            struct restart_info *restart,
                            const struct funcinst *func)
{
        restart->restart_type = RESTART_HOSTFUNC;
        struct restart_hostfunc *hf = &restart->restart_u.hostfunc;
        const struct functype *ft = funcinst_functype(func);
        hf->func = ctx->event_u.call.func; /* caller hostfunc */
        assert(hf->func->is_host);

        /*
         * update the bottom of the stack so that return_to_hostfunc
         * will be called when returning from this call.
         */
        hf->saved_bottom = ctx->bottom;
        ctx->bottom = ctx->frames.lsize;

        /*
         * a note aboutstack adjustment
         *
         * return_to_hostfunc adjusts the operand stack pointer by
         * this value so that the caller host function can still access
         * its parameters after a restart.
         *
         * the caller hostfunc can pop the return values of the callee
         * function (`func`) as the following:
         *
         *    ctx->stack.lsize += hf->stack_adj;
         *    exec_pop_vals(ctx, &ft->result, r);
         */
        hf->stack_adj = resulttype_cellsize(&ft->result);

        /*
         * make restart possibly nest
         */
        ctx->restarts.lsize++;

        /*
         * schedule_call
         */
        ctx->event_u.call.func = func;
        ctx->event = EXEC_EVENT_CALL;
        return ETOYWASMRESTART;
}
