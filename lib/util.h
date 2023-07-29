#include <stddef.h>
#include <stdint.h>

#include "platform.h"

#define ARRAYCOUNT(a) (sizeof(a) / sizeof(a[0]))

int __must_check resize_array(void **p, size_t elem_size,
                              uint32_t new_elem_count);
#if defined(__NuttX__)
#include <stdlib.h> /* NuttX has zalloc() in stdlib */
#else
void *__must_check zalloc(size_t sz) __malloc_like __alloc_size(1);
#endif

#define ARRAY_RESIZE(a, sz) resize_array((void **)&(a), sizeof(*a), sz)
#define ARRAY_FOREACH(p, a, sz) for (p = a; p < a + sz; p++)

#define ZERO(p) memset(p, 0, sizeof(*p))

#define HOWMANY(a, b) ((a + (b - 1)) / b)

char *xstrnstr(const char *haystack, const char *needle, size_t len);
