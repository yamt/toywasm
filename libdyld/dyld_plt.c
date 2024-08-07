#include <assert.h>
#include <inttypes.h>

#include "dyld.h"
#include "dyld_impl.h"
#include "dyld_plt.h"
#include "exec_context.h"
#include "instance.h"
#include "xlog.h"

int
dyld_resolve_plt(struct exec_context *ectx, struct dyld_plt *plt)
{
        struct dyld_object *refobj = plt->refobj;
        struct dyld *d = refobj->dyld;
        const struct name *sym = plt->sym;
        int ret;
        uint32_t addr;
        ret = dyld_resolve_symbol(plt->refobj, SYM_TYPE_FUNC, sym, &addr);
        if (ret != 0) {
                xlog_error("dyld: PLT failed to resolve %.*s %.*s",
                           CSTR(refobj->name), CSTR(sym));
                return ret;
        }
        const struct functype *ft = funcinst_functype(&plt->pltfi);
        ret = table_get_func(ectx, d->tableinst, addr, ft, &plt->finst);
        if (ret != 0) {
                return ret;
        }
        xlog_trace("dyld: PLT resolved %.*s %.*s to addr %08" PRIx32
                   " finst %p",
                   CSTR(refobj->name), CSTR(sym), addr, (void *)plt->finst);
        return 0;
}

int
dyld_plt(struct exec_context *ctx, struct host_instance *hi,
         const struct functype *ft, const struct cell *params,
         struct cell *results)
{
        struct dyld_plt *plt = (void *)hi;
        if (plt->finst == NULL) {
                /*
                 * resolve the symbol.
                 */
                assert(!compare_functype(funcinst_functype(&plt->pltfi), ft));
                int ret = dyld_resolve_plt(ctx, plt);
                if (ret != 0) {
                        return ret;
                }
        }

#if 0 /* a bit dirty */
        /*
         * modify our funcinst so that next call will have no PLT overhead.
         * if you enable this, you need to relax the assertion in
         * funcinst_func.
         */
        plt->pltfi = *plt->finst;
#endif

        /*
         * set up a call to the resolved function.
         *
         * note that we (PLT wrapper) and the resolved function share
         * the same function type. this is a tail-call.
         *
         * we rewind our host frame by returning a restartable error.
         * the main interpreter loop will notice the event and call
         * the target function for us.
         *
         * if our wasm-level caller was calling us with a tail-call
         * instruction like `return_call`, it's important to rewind
         * our host frame here to maintain the tail-call guarantee.
         *
         * also, this approach simplifies exception propagation.
         * (toywasm doesn't have an embedder API to catch and rethrow
         * an exception as of writing this.)
         */
        ctx->event_u.call.func = plt->finst;
        ctx->event = EXEC_EVENT_CALL;
        return ETOYWASMRESTART;
}
