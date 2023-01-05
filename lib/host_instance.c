#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>

#include "host_instance.h"
#include "instance.h"
#include "type.h"
#include "xlog.h"

static void
_dtor(struct import_object *im)
{
        struct funcinst *fis = im->dtor_arg;
        if (fis != NULL) {
                uint32_t nfuncs = im->nentries;
                uint32_t i;
                for (i = 0; i < nfuncs; i++) {
                        struct funcinst *fi = &fis[i];
                        if (fi->u.host.type != NULL) {
                                functype_free(fi->u.host.type);
                        }
                }
                free(fis);
        }
}

int
import_object_create_for_host_funcs(const struct host_module *modules,
                                    size_t n, struct host_instance *hi,
                                    struct import_object **impp)
{
        struct import_object *im;
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
        ret = import_object_alloc(nfuncs, &im);
        if (ret != 0) {
                goto fail;
        }
        fis = zalloc(nfuncs * sizeof(*fis));
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
                        ret = functype_from_string(func->type, &ft);
                        if (ret != 0) {
                                xlog_error("failed to parse functype: %s",
                                           func->type);
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
                        e->type = IMPORT_FUNC;
                        e->u.func = fi;
                        idx++;
                }
        }
        assert(idx == nfuncs);
        *impp = im;
        return 0;
fail:
        import_object_destroy(im);
        return ret;
}

void
host_func_dump_params(const struct functype *ft, const struct cell *params)
{
        const struct resulttype *rt = &ft->parameter;
        uint32_t i;
        for (i = 0; i < rt->ntypes; i++) {
                enum valtype type = rt->types[i];
                uint32_t sz = valtype_cellsize(type);
                struct val val;
                val_from_cells(&val, &params[i], sz);
#if defined(TOYWASM_USE_SMALL_CELLS)
                switch (sz) {
                case 1:
                        xlog_trace("param[%" PRIu32 "] = %08" PRIu32, i,
                                   val.u.i32);
                        break;
                case 2:
                        xlog_trace("param[%" PRIu32 "] = %016" PRIu64, i,
                                   val.u.i64);
                        break;
                }
#else
                xlog_trace("param[%" PRIu32 "] = %016" PRIu64, i, val.u.i64);
#endif
        }
}
