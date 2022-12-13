#include <assert.h>
#include <errno.h>
#include <string.h>

#include "idalloc.h"

#define FREE_SLOT ((void *)1)

static void **
idalloc_getptr(struct idalloc *ida, uint32_t id)
{
        assert(ida->base <= id);
        id -= ida->base;
        assert(id < ida->vec.lsize);
        return &VEC_ELEM(ida->vec, id);
}

void
idalloc_init(struct idalloc *ida, uint32_t minid, uint32_t maxid)
{
        assert(minid <= maxid);
        memset(ida, 0, sizeof(*ida));
        ida->base = minid;
        ida->maxid = maxid - minid;
}

void
idalloc_destroy(struct idalloc *ida)
{
        VEC_FREE(ida->vec);
}

int
idalloc_alloc(struct idalloc *ida, uint32_t *idp)
{
        void **it;
        uint32_t id;
        int ret;
        VEC_FOREACH_IDX(id, it, ida->vec) {
                if (*it == FREE_SLOT) {
                        *it = NULL;
                        *idp = ida->base + id;
                        return 0;
                }
        }
        id = ida->vec.lsize;
        if (ida->maxid < id) {
                return ERANGE;
        }
        ret = VEC_RESIZE(ida->vec, id + 1);
        if (ret != 0) {
                return ret;
        }
        VEC_ELEM(ida->vec, id) = NULL;
        *idp = ida->base + id;
        return 0;
}

void
idalloc_free(struct idalloc *ida, uint32_t id)
{
        assert(idalloc_test(ida, id));
        *idalloc_getptr(ida, id) = FREE_SLOT;
}

bool
idalloc_test(struct idalloc *ida, uint32_t id)
{
        if (id < ida->base) {
                return false;
        }
        if (id >= ida->base + ida->vec.lsize) {
                return false;
        }
        return *idalloc_getptr(ida, id) != FREE_SLOT;
}

void
idalloc_set_user(struct idalloc *ida, uint32_t id, void *user_data)
{
        assert(idalloc_test(ida, id));
        assert(user_data != FREE_SLOT);
        *idalloc_getptr(ida, id) = user_data;
}

void *
idalloc_get_user(struct idalloc *ida, uint32_t id)
{
        assert(idalloc_test(ida, id));
        void *p = *idalloc_getptr(ida, id);
        assert(p != FREE_SLOT);
        return p;
}
