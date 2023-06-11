#if !defined(_BITMAP_H)
#define _BITMAP_H

#include <stdbool.h>
#include <stdint.h>

struct bitmap {
        uint32_t *data;
};

int bitmap_alloc(struct bitmap *b, uint32_t n);
void bitmap_free(struct bitmap *b);
void bitmap_set(struct bitmap *b, uint32_t idx);
bool bitmap_test(struct bitmap *b, uint32_t idx);

#endif /* !defined(_BITMAP_H) */
