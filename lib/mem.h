#include <stddef.h>

#include "platform.h"

struct mem_context {
        _Atomic size_t allocated;
        size_t limit;
        struct mem_context *parent;
};

__BEGIN_EXTERN_C

void mem_context_init(struct mem_context *ctx);
void mem_context_clear(struct mem_context *ctx);
void *__must_check mem_alloc(struct mem_context *ctx, size_t sz) __malloc_like
        __alloc_size(2);
void *__must_check mem_zalloc(struct mem_context *ctx, size_t sz) __malloc_like
        __alloc_size(2);
void *__must_check mem_calloc(struct mem_context *ctx, size_t a, size_t b);
void mem_free(struct mem_context *ctx, void *p, size_t sz);
void *__must_check mem_resize(struct mem_context *ctx, void *p, size_t oldsz,
                              size_t newsz) __malloc_like __alloc_size(4);

__END_EXTERN_C
