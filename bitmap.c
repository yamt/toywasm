#include <errno.h>
#include <stdlib.h>

#include "bitmap.h"
#include "util.h"

int
bitmap_alloc(struct bitmap *b, uint32_t n)
{
        b->data = calloc(HOWMANY(n, 32), sizeof(uint32_t));
        if (b->data == NULL) {
                return ENOMEM;
        }
        return 0;
}

void
bitmap_free(struct bitmap *b)
{
        free(b->data);
}

void
bitmap_set(struct bitmap *b, uint32_t idx)
{
        uint32_t mask = 1U << (idx % 32);
        b->data[idx / 32] |= mask;
}

bool
bitmap_test(struct bitmap *b, uint32_t idx)
{
        uint32_t mask = 1U << (idx % 32);
        return (b->data[idx / 32] & mask) != 0;
}
