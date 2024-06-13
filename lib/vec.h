#if !defined(_TOYWASM_VEC_H)
#define _TOYWASM_VEC_H

#include <stdint.h>

#include "platform.h"
#include "util.h"

struct mem_context;

#define VEC(TAG, TYPE)                                                        \
        struct TAG {                                                          \
                TYPE *p;                                                      \
                uint32_t lsize;                                               \
                uint32_t psize;                                               \
        }

__BEGIN_EXTERN_C

int __must_check _vec_resize(struct mem_context *ctx, void *vec, size_t elem_size,
                             uint32_t new_elem_count);
int __must_check _vec_prealloc(struct mem_context *ctx, void *vec, size_t elem_size, uint32_t count);
void _vec_free(struct mem_context *ctx, void *vec);

__END_EXTERN_C

#define VEC_INIT(v) memset(&v, 0, sizeof(v))
/*
 * VEC_RESIZE resizes the size of vector. that is, psize == lsize == sz.
 * when extending the vector, it initializes newly allocated elements
 * with zeros.
 */
#define VEC_RESIZE(ctx, v, sz) _vec_resize(ctx, &v, sizeof(*v.p), sz);
/*
 * VEC_PREALLOC ensures the vector to have enough tail room to store the
 * given number of elements. that is, psize >= lsize + needed.
 * it doesn't initialize newly allocated elements.
 */
#define VEC_PREALLOC(ctx, v, needed) _vec_prealloc(ctx, &v, sizeof(*v.p), needed);
#define VEC_FREE(ctx, v) _vec_free(ctx, &v)
#define VEC_FOREACH(it, v) ARRAY_FOREACH(it, v.p, v.lsize)
#define VEC_FOREACH_IDX(i, it, v) for (i = 0, it = v.p; i < v.lsize; i++, it++)
#define VEC_ELEM(v, idx) (v.p[idx])
#define VEC_LASTELEM(v) (v.p[v.lsize - 1])
#define VEC_NEXTELEM(v) (v.p[v.lsize])

#define VEC_PUSH(v) (&v.p[v.lsize++])
#define VEC_POP(v) (&v.p[--v.lsize])
#define VEC_POP_DROP(v) v.lsize--

#endif /* !defined(_TOYWASM_VEC_H) */
