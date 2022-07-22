#if !defined(_VEC_H)
#define _VEC_H

#include <stdint.h>

#include "util.h"

#define VEC(TAG, TYPE)                                                        \
        struct TAG {                                                          \
                TYPE *p;                                                      \
                uint32_t lsize;                                               \
                uint32_t psize;                                               \
        }

int _vec_resize(void *vec, size_t elem_size, uint32_t new_elem_count);
int _vec_prealloc(void *vec, size_t elem_size, uint32_t tailroom);
void _vec_free(void *vec);

#define VEC_RESIZE(v, sz) _vec_resize(&v, sizeof(*v.p), sz);
#define VEC_PREALLOC(v, sz) _vec_prealloc(&v, sizeof(*v.p), sz);
#define VEC_FREE(v) _vec_free(&v)
#define VEC_FOREACH(it, v) ARRAY_FOREACH(it, v.p, v.lsize)
#define VEC_FOREACH_IDX(i, it, v) for (i = 0, it = v.p; i < v.lsize; i++, it++)
#define VEC_ELEM(v, idx) (v.p[idx])
#define VEC_LASTELEM(v) (v.p[v.lsize - 1])

#define VEC_PUSH(v) (&v.p[v.lsize++])
#define VEC_POP(v) (&v.p[--v.lsize])
#define VEC_POP_DROP(v) v.lsize--

#endif /* !defined(_VEC_H) */
