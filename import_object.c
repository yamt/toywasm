#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "instance.h"
#include "type.h"
#include "xlog.h"

int
import_object_alloc(uint32_t nentries, struct import_object **resultp)
{
        struct import_object *im;

        im = zalloc(sizeof(*im));
        if (im == NULL) {
                return ENOMEM;
        }
        im->nentries = nentries;
        im->entries = zalloc(nentries * sizeof(*im->entries));
        if (im->entries == NULL) {
                free(im);
                return ENOMEM;
        }
        *resultp = im;
        return 0;
}

int
import_object_create_for_exports(struct instance *inst,
                                 const struct name *module_name,
                                 struct import_object **resultp)
{
        struct module *m = inst->module;
        struct import_object *im;
        int ret;

        ret = import_object_alloc(m->nexports, &im);
        if (ret != 0) {
                return ret;
        }
        uint32_t i;
        for (i = 0; i < m->nexports; i++) {
                const struct export *ex = &m->exports[i];
                const struct exportdesc *d = &ex->desc;
                struct import_object_entry *e = &im->entries[i];
                switch (d->type) {
                case EXPORT_FUNC:
                        e->u.func = VEC_ELEM(inst->funcs, d->idx);
                        e->type = IMPORT_FUNC;
                        break;
                case EXPORT_TABLE:
                        e->u.table = VEC_ELEM(inst->tables, d->idx);
                        e->type = IMPORT_TABLE;
                        break;
                case EXPORT_MEMORY:
                        e->u.mem = VEC_ELEM(inst->mems, d->idx);
                        e->type = IMPORT_MEMORY;
                        break;
                case EXPORT_GLOBAL:
                        e->u.global = VEC_ELEM(inst->globals, d->idx);
                        e->type = IMPORT_GLOBAL;
                        break;
                default:
                        assert(false);
                }
                e->module_name = module_name;
                e->name = &ex->name;
                xlog_trace("created an entry for %s:%s", e->module_name,
                           e->name);
        }
        im->next = NULL;
        *resultp = im;
        return 0;
}

void
import_object_destroy(struct import_object *im)
{
        if (im->dtor != NULL) {
                im->dtor(im);
        }
        free(im->entries);
        free(im);
}
