#include <stddef.h>
#include <stdint.h>

#define ARRAYCOUNT(a) (sizeof(a) / sizeof(a[0]))

int resize_array(void **p, size_t elem_size, uint32_t new_elem_count);
void *zalloc(size_t sz);

#define ARRAY_RESIZE(a, sz) resize_array((void **)&(a), sizeof(*a), sz)
#define ARRAY_FOREACH(p, a, sz) for (p = a; p < a + sz; p++)

#define ZERO(p) memset(p, 0, sizeof(*p))

#define HOWMANY(a, b) ((a + (b - 1)) / b)
