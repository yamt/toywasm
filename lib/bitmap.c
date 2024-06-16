#include <errno.h>
#include <stdlib.h>

#include "bitmap.h"
#include "mem.h"
#include "util.h"

int
bitmap_alloc(struct mem_context *mctx, struct bitmap *b, uint32_t n)
{
        void *p = NULL;
        if (n > 0) {
                p = mem_calloc(mctx, HOWMANY(n, 32), sizeof(uint32_t));
                if (p == NULL) {
                        return ENOMEM;
                }
        }
        b->data = p;
        return 0;
}

void
bitmap_free(struct mem_context *mctx, struct bitmap *b, uint32_t n)
{
        mem_free(mctx, b->data, HOWMANY(n, 32) * sizeof(uint32_t));
}

void
bitmap_set(struct bitmap *b, uint32_t idx)
{
        uint32_t mask = 1U << (idx % 32);
        b->data[idx / 32] |= mask;
}

bool
bitmap_test(const struct bitmap *b, uint32_t idx)
{
        uint32_t mask = 1U << (idx % 32);
        return (b->data[idx / 32] & mask) != 0;
}
