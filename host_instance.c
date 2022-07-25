#include <errno.h>
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
import_object_create_for_host_funcs(const char *module_name,
                                    const struct host_func *funcs,
                                    size_t nfuncs, struct host_instance *hi,
                                    struct import_object **impp)
{
        struct import_object *im;
        struct funcinst *fis = NULL;
        int ret;
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
        size_t i;
        for (i = 0; i < nfuncs; i++) {
                struct functype *ft;
                ret = functype_from_string(funcs[i].type, &ft);
                if (ret != 0) {
                        xlog_error("failed to pare functype: %s",
                                   funcs[i].type);
                        goto fail;
                }
                struct funcinst *fi = &fis[i];
                fi->is_host = true;
                fi->u.host.func = funcs[i].func;
                fi->u.host.type = ft;
                fi->u.host.instance = hi;
                struct import_object_entry *e = &im->entries[i];
                e->module_name = module_name;
                e->name = funcs[i].name;
                e->type = IMPORT_FUNC;
                e->u.func = fi;
        }
        *impp = im;
        return 0;
fail:
        import_object_destroy(im);
        return ret;
}
