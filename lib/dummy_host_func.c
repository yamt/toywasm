#include <assert.h>
#include <errno.h>

#include "exec.h"
#include "host_instance.h"
#include "instance.h"
#include "mem.h"
#include "type.h"

static int
dummy_func(struct exec_context *ectx, struct host_instance *hi,
           const struct functype *ft, const struct cell *params,
           struct cell *results)
{
        const struct import *im = (void *)hi;
        return trap_with_id(ectx, TRAP_UNRESOLVED_IMPORTED_FUNC,
                            "unresolved imported function %.*s:%.*s is called",
                            CSTR(&im->module_name), CSTR(&im->name));
}

static void
_dtor(struct mem_context *mctx, struct import_object *im)
{
        struct funcinst *fis = im->dtor_arg;
        if (fis != NULL) {
                mem_free(mctx, fis, im->nentries * sizeof(*fis));
        }
}

int
create_satisfying_functions(struct mem_context *mctx, const struct module *m,
                            struct import_object **imop)
{
        struct import_object *imo = NULL;
        struct funcinst *fis = NULL;
        uint32_t nfuncs;
        uint32_t i;
        struct meminst *mi = NULL;
        int ret;

        nfuncs = 0;
        for (i = 0; i < m->nimports; i++) {
                const struct import *im = &m->imports[i];
                if (im->desc.type != EXTERNTYPE_FUNC) {
                        continue;
                }
                nfuncs++;
        }

        ret = import_object_alloc(mctx, nfuncs, &imo);
        if (ret != 0) {
                goto fail;
        }
        fis = mem_zalloc(mctx, nfuncs * sizeof(*fis));
        if (fis == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        imo->dtor = _dtor;
        imo->dtor_arg = fis;

        uint32_t idx = 0;
        for (i = 0; i < m->nimports; i++) {
                const struct import *im = &m->imports[i];
                if (im->desc.type != EXTERNTYPE_FUNC) {
                        continue;
                }
                uint32_t typeidx = im->desc.u.typeidx;
                assert(typeidx < m->ntypes);
                const struct functype *ft = &m->types[typeidx];
                assert(idx < imo->nentries);
                struct funcinst *fi = &fis[idx];
                fi->is_host = true;
                fi->u.host.func = dummy_func;
                fi->u.host.type = ft;
                fi->u.host.instance = (void *)im;
                struct import_object_entry *e = &imo->entries[idx++];
                e->module_name = &im->module_name;
                e->name = &im->name;
                e->type = EXTERNTYPE_FUNC;
                e->u.func = fi;
        }

        *imop = imo;
        return 0;
fail:
        if (imo != NULL) {
                import_object_destroy(mctx, imo);
        }
        if (mi != NULL) {
                memory_instance_destroy(mctx, mi);
        }
        return ret;
}
