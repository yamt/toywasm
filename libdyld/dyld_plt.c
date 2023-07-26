#include <assert.h>
#include <inttypes.h>

#include "dyld.h"
#include "dyld_plt.h"
#include "exec.h"
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

        ctx->event_u.call.func = plt->finst;
        ctx->event = EXEC_EVENT_CALL;
        return ETOYWASMRESTART;
}
