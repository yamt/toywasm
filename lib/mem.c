#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if !defined(NDEBUG) && defined(__APPLE__)
#include <malloc/malloc.h>
#endif

#include "mem.h"

#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
static void
mem_unreserve_one(struct mem_context *ctx, size_t diff)
{
        assert(ctx->allocated <= ctx->limit);
        assert(ctx->allocated >= diff);
        size_t ov = atomic_fetch_sub(&ctx->allocated, diff);
        assert(ov >= diff);
}

static int
mem_reserve_one(struct mem_context *ctx, size_t diff)
{
        size_t ov;
        size_t nv;
        do {
                ov = ctx->allocated;
                assert(ov <= ctx->limit);
                if (ctx->limit - ov < diff) {
                        return ENOMEM;
                }
                nv = ov + diff;
        } while (!atomic_compare_exchange_weak(&ctx->allocated, &ov, nv));
#if defined(TOYWASM_ENABLE_HEAP_TRACKING_PEAK)
        size_t opeak;
        size_t npeak;
        do {
                opeak = ctx->peak;
                if (opeak >= nv) {
                        break;
                }
                npeak = nv;
        } while (!atomic_compare_exchange_weak(&ctx->peak, &opeak, npeak));
#endif
        return 0;
}
#endif

static void
mem_unreserve(struct mem_context *ctx, size_t diff)
{
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
        mem_unreserve_one(ctx, diff);
        if (ctx->parent != NULL) {
                mem_unreserve(ctx->parent, diff);
        }
#endif
}

static int
mem_reserve(struct mem_context *ctx, size_t diff)
{
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
        int ret;
        ret = mem_reserve_one(ctx, diff);
        if (ret != 0) {
                return ret;
        }
        if (ctx->parent != NULL) {
                ret = mem_reserve(ctx->parent, diff);
                if (ret != 0) {
                        mem_unreserve_one(ctx, diff); /* undo */
                        return ret;
                }
        }
#endif
        return 0;
}

static void
assert_malloc_size(void *p, size_t sz)
{
#if !defined(NDEBUG) && defined(__APPLE__)
        size_t msz = malloc_size(p);
        assert(msz == sz);
#endif
}

void
mem_context_init(struct mem_context *ctx)
{
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
        ctx->allocated = 0;
        ctx->limit = SIZE_MAX;
#if defined(TOYWASM_ENABLE_HEAP_TRACKING_PEAK)
        ctx->peak = 0;
#endif
#endif
        ctx->parent = NULL;
}

void
mem_context_clear(struct mem_context *ctx)
{
        assert(ctx->allocated == 0);
}

int
mem_context_setlimit(struct mem_context *ctx, size_t limit)
{
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
        if (limit < ctx->allocated) {
                return ENOMEM;
        }
        ctx->limit = limit;
#endif
        return 0;
}

void *
mem_alloc(struct mem_context *ctx, size_t sz)
{
        if (mem_reserve(ctx, sz)) {
                return NULL;
        }
        assert(sz > 0);
        void *p = malloc(sz);
        if (p == NULL) {
                return NULL;
        }
        return p;
}

void *
mem_zalloc(struct mem_context *ctx, size_t sz)
{
        void *p = mem_alloc(ctx, sz);
        if (p != NULL) {
                memset(p, 0, sz);
        }
        return p;
}

void *
mem_calloc(struct mem_context *ctx, size_t a, size_t b)
{
        assert(a > 0);
        assert(b > 0);
        size_t sz = a * b;
        if (sz / a != b) {
                return NULL;
        }
        return mem_zalloc(ctx, sz);
}

void
mem_free(struct mem_context *ctx, void *p, size_t sz)
{
        if (p == NULL) {
                return;
        }
        assert(sz > 0);
        assert_malloc_size(p, sz);
        free(p);
        mem_unreserve(ctx, sz);
}

void *
mem_extend(struct mem_context *ctx, void *p, size_t oldsz, size_t newsz)
{
        if (p != NULL) {
                assert(oldsz > 0);
                assert_malloc_size(p, oldsz);
        } else {
                assert(oldsz == 0);
        }
        assert(oldsz < newsz);
        size_t diff = newsz - oldsz;
        if (mem_reserve(ctx, diff)) {
                return NULL;
        }
        void *np = realloc(p, newsz);
        if (np == NULL) {
                return NULL;
        }
        return np;
}

void *
mem_extend_zero(struct mem_context *ctx, void *p, size_t oldsz, size_t newsz)
{
        uint8_t *np = mem_extend(ctx, p, oldsz, newsz);
        if (np != NULL) {
                memset(np + oldsz, 0, newsz - oldsz);
        }
        return np;
}

void *
mem_shrink(struct mem_context *ctx, void *p, size_t oldsz, size_t newsz)
{
        assert(p != NULL);
        assert(oldsz > newsz);
        assert_malloc_size(p, oldsz);
        size_t diff = oldsz - newsz;
        mem_unreserve(ctx, diff);
        void *np = realloc(p, newsz);
        if (np == NULL) {
                return NULL;
        }
        return np;
}
