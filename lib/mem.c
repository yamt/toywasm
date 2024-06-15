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

static void
mem_unreserve(struct mem_context *ctx, size_t diff)
{
        assert(ctx->allocated <= ctx->limit);
        assert(ctx->allocated >= diff);
        size_t ov = atomic_fetch_sub(&ctx->allocated, diff);
        assert(ov >= diff);
        if (ctx->parent != NULL) {
                mem_unreserve(ctx->parent, diff);
        }
}

static int
mem_reserve(struct mem_context *ctx, size_t diff)
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
        if (ctx->parent != NULL) {
                int ret = mem_reserve(ctx->parent, diff);
                if (ret != 0) {
                        mem_unreserve(ctx, diff);
                        return ret;
                }
        }
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
        ctx->allocated = 0;
        ctx->limit = SIZE_MAX;
        ctx->parent = NULL;
}

void
mem_context_clear(struct mem_context *ctx)
{
        assert(ctx->allocated == 0);
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
mem_resize(struct mem_context *ctx, void *p, size_t oldsz, size_t newsz)
{
        if (p != NULL) {
                assert(oldsz > 0);
                assert_malloc_size(p, oldsz);
        } else {
                assert(oldsz == 0);
        }
        if (oldsz < newsz) {
                size_t diff = newsz - oldsz;
                if (mem_reserve(ctx, diff)) {
                        return NULL;
                }
        } else {
                size_t diff = oldsz - newsz;
                mem_unreserve(ctx, diff);
        }
        void *np = realloc(p, newsz);
        if (np == NULL) {
                return NULL;
        }
        return np;
}
