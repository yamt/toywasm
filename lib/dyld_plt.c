#include <assert.h>

#include "dyld.h"
#include "dyld_plt.h"
#include "exec.h"

int
dyld_plt(struct exec_context *ctx, struct host_instance *hi,
         const struct functype *ft, const struct cell *params,
         struct cell *results)
{
        /* resolve the index */
        struct dyld_plt *plt = (void *)hi;
        if (plt->finst == NULL) {
                struct dyld *d = plt->dyld;
                int ret;
                uint32_t addr;
                ret = dyld_resolve_symbol(d, plt->refobj, SYM_TYPE_FUNC,
                                          plt->sym, &addr);
                if (ret != 0) {
                        return ret;
                }
                struct val val;
                table_get(d->tableinst, addr, &val);
                plt->finst = val.u.funcref.func;
        }

        ctx->event_u.call.func = plt->finst;
        ctx->event = EXEC_EVENT_CALL;

        /* cancel the lsize adustment in do_host_call */
        uint32_t nparams = resulttype_cellsize(&ft->parameter);
        uint32_t nresults = resulttype_cellsize(&ft->result);
        ctx->stack.lsize += nparams;
        ctx->stack.lsize -= nresults;

        return 0;
}
