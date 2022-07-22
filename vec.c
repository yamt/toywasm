#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "vec.h"

VEC(_vec, void *);

int
_vec_resize(void *vec, size_t elem_size, uint32_t new_elem_count)
{
        struct _vec *v = vec;
        int ret;
        if (new_elem_count > v->psize) {
                ret = resize_array((void **)&v->p, elem_size, new_elem_count);
                if (ret != 0) {
                        return ret;
                }
                v->psize = new_elem_count;
        }
        if (new_elem_count > v->lsize) {
                memset(v->p + v->lsize * elem_size, 0,
                       (new_elem_count - v->lsize) * elem_size);
        }
        v->lsize = new_elem_count;
        return 0;
}

int
_vec_prealloc(void *vec, size_t elem_size, uint32_t count)
{
        struct _vec *v = vec;
        int ret;
        uint32_t need = v->lsize + count;
        if (need > v->psize) {
                ret = resize_array((void **)&v->p, elem_size, need);
                if (ret != 0) {
                        return ret;
                }
                v->psize = need;
        }
        return 0;
}

void
_vec_free(void *vec)
{
        struct _vec *v = vec;
        free(v->p);
        v->p = NULL;
        v->lsize = 0;
        v->psize = 0;
}
