#if !defined(_TOYWASM_BITMAP_H)
#define _TOYWASM_BITMAP_H

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

struct bitmap {
        uint32_t *data;
};

__BEGIN_EXTERN_C

/*
 * Note: a bitmap initalized by bitmap_alloc has all bits cleared.
 */

struct mem_context;
int bitmap_alloc(struct mem_context *mctx, struct bitmap *b, uint32_t n);
void bitmap_free(struct mem_context *mctx, struct bitmap *b, uint32_t n);
void bitmap_set(struct bitmap *b, uint32_t idx);
bool bitmap_test(const struct bitmap *b, uint32_t idx);

__END_EXTERN_C

#endif /* !defined(_TOYWASM_BITMAP_H) */
