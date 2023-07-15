#include <assert.h>
#include <inttypes.h>

#include "dyld.h"
#include "dyld_plt.h"
#include "exec.h"
#include "xlog.h"

int
dyld_plt(struct exec_context *ctx, struct host_instance *hi,
         const struct functype *ft, const struct cell *params,
         struct cell *results)
{
        struct dyld_plt *plt = (void *)hi;
        if (plt->finst == NULL) {
                struct dyld *d = plt->dyld;
                struct dyld_object *refobj = plt->refobj;
                const struct name *objname = dyld_object_name(refobj);
                const struct name *sym = plt->sym;
                int ret;
                uint32_t addr;
                ret = dyld_resolve_symbol(d, plt->refobj, SYM_TYPE_FUNC, sym,
                                          &addr);
                if (ret != 0) {
                        xlog_error("dyld: PLT failed to resolve %.*s %.*s",
                                   CSTR(objname), CSTR(sym));
                        return ret;
                }
                struct val val;
                table_get(d->tableinst, addr, &val);
                plt->finst = val.u.funcref.func;
                xlog_trace("dyld: PLT resolved %.*s %.*s to addr %08" PRIx32
                           " finst %p",
                           CSTR(objname), CSTR(sym), addr, (void *)plt->finst);
        }

        ctx->event_u.call.func = plt->finst;
        ctx->event = EXEC_EVENT_CALL;
        return ETOYWASMRESTART;
}