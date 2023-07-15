#include "dyld.h"

int
dyld_plt(struct exec_context *ctx, struct host_instance *hi,
         const struct functype *ft, const struct cell *params,
         struct cell *results)
{
        /* resolve the index */
        struct plt *plt = (struct plt *)hi;
        if (plt->finst == NULL) {
                ret = dyld_symbol_resolve(dyld, plt->sym);

                ret = table_get_func_indirect(ctx, tinst, ft,
                                              plt->idx_in_table, &plt->finst);
                if (ret != 0) {
                        return ret;
                }
                assert(plt->finst != NULL);
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
