#include <assert.h>

#include "dyld.h"
#include "dyld_plt.h"
#include "exec_context.h"

int
dyld_plt(struct exec_context *ctx, struct host_instance *hi,
         const struct functype *ft, const struct cell *params,
         struct cell *results)
{
        /* resolve the index */
        struct dyld_plt *plt = (void *)hi;
        if (plt->finst == NULL) {
                int ret;
                const void *p;
                ret = dyld_resolve_symbol(plt->dyld, plt->refobj, EXPORT_FUNC,
                                          plt->sym, &p);
                if (ret != 0) {
                        return ret;
                }
                plt->finst = p;
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
