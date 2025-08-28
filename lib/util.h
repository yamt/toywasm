#include <stddef.h>
#include <stdint.h>

#include "platform.h"

#define ARRAYCOUNT(a) (sizeof(a) / sizeof(a[0]))

void *__must_check xzalloc(size_t sz) __malloc_like __alloc_size(1);

struct mem_context;
int __must_check array_extend(struct mem_context *ctx, void **p,
                              size_t elem_size, uint32_t old_elem_count,
                              uint32_t new_elem_count);
int __must_check array_shrink(struct mem_context *ctx, void **p,
                              size_t elem_size, uint32_t old_elem_count,
                              uint32_t new_elem_count);

#define ARRAY_EXTEND(ctx, a, osz, sz)                                         \
        array_extend((ctx), (void **)&(a), sizeof(*a), osz, sz)
#define ARRAY_FOREACH(p, a, sz) for (p = a; p < a + sz; p++)

#define ZERO(p) memset(p, 0, sizeof(*p))

#define HOWMANY(a, b) ((a + (b - 1)) / b)

/*
 * strnstr is a FreeBSD extension. xstrnstr is a wrapper of strnstr
 * with a fallback implementation for other platforms.
 */
char *xstrnstr(const char *haystack, const char *needle, size_t len);
