#include <assert.h>

#include "instance.h"
#include "type.h"

static void
_dtor(struct mem_context *mctx, struct import_object *imo)
{
        uint32_t i;
        for (i = 0; i < imo->nentries; i++) {
                struct import_object_entry *e = &imo->entries[i];
                struct meminst *mi = e->u.mem;
                if (mi != NULL) {
                        memory_instance_destroy(mi->mctx, mi);
                }
        }
}

int
create_satisfying_shared_memories(struct mem_context *mctx,
                                  struct mem_context *instance_mctx,
                                  const struct module *m,
                                  struct import_object **imop)
{
        struct import_object *imo = NULL;
        uint32_t nsharedimports;
        uint32_t i;
        struct meminst *mi = NULL;
        int ret;

        nsharedimports = 0;
        for (i = 0; i < m->nimports; i++) {
                const struct import *im = &m->imports[i];
                if (im->desc.type != EXTERNTYPE_MEMORY) {
                        continue;
                }
                const struct memtype *mt = &im->desc.u.memtype;
                if ((mt->flags & MEMTYPE_FLAG_SHARED) == 0) {
                        continue;
                }
                nsharedimports++;
        }

        ret = import_object_alloc(mctx, nsharedimports, &imo);
        if (ret != 0) {
                goto fail;
        }
        imo->dtor = _dtor;

        uint32_t idx = 0;
        for (i = 0; i < m->nimports; i++) {
                const struct import *im = &m->imports[i];
                if (im->desc.type != EXTERNTYPE_MEMORY) {
                        continue;
                }
                const struct memtype *mt = &im->desc.u.memtype;
                if ((mt->flags & MEMTYPE_FLAG_SHARED) == 0) {
                        continue;
                }
                ret = memory_instance_create(instance_mctx, &mi, mt);
                if (ret != 0) {
                        goto fail;
                }
                assert(idx < imo->nentries);
                struct import_object_entry *e = &imo->entries[idx++];
                e->module_name = &im->module_name;
                e->name = &im->name;
                e->type = EXTERNTYPE_MEMORY;
                e->u.mem = mi;
        }

        *imop = imo;
        return 0;
fail:
        if (imo != NULL) {
                import_object_destroy(mctx, imo);
        }
        if (mi != NULL) {
                memory_instance_destroy(instance_mctx, mi);
        }
        return ret;
}
