#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "vec.h"

VEC(_vec, void);

int
_vec_resize(struct mem_context *mctx, void *vec, size_t elem_size,
            uint32_t new_elem_count)
{
        struct _vec *v = vec;
        int ret;
        assert(v->lsize <= v->psize);
        assert((v->psize == 0) == (v->p == NULL));
        if (new_elem_count > v->psize) {
                ret = resize_array(mctx, (void **)&v->p, elem_size, v->psize,
                                   new_elem_count);
                if (ret != 0) {
                        return ret;
                }
                v->psize = new_elem_count;
        }
        if (new_elem_count > v->lsize) {
                memset((uint8_t *)v->p + v->lsize * elem_size, 0,
                       (new_elem_count - v->lsize) * elem_size);
        }
        v->lsize = new_elem_count;
        return 0;
}

int
_vec_prealloc(struct mem_context *mctx, void *vec, size_t elem_size,
              uint32_t count)
{
        struct _vec *v = vec;
        int ret;
        uint32_t need = v->lsize + count;
        if (need > v->psize) {
                ret = resize_array(mctx, (void **)&v->p, elem_size, v->psize,
                                   need);
                if (ret != 0) {
                        return ret;
                }
                v->psize = need;
        }
        return 0;
}

void
_vec_free(struct mem_context *mctx, void *vec, size_t elem_size)
{
        struct _vec *v = vec;
        int ret = resize_array(mctx, (void **)&v->p, elem_size, v->psize, 0);
        assert(ret == 0);
        v->p = NULL;
        v->lsize = 0;
        v->psize = 0;
}
