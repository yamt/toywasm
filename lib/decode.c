#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "decode.h"
#include "endian.h"
#include "leb128.h"
#include "util.h"

int
read_u8(const uint8_t **pp, const uint8_t *ep, uint8_t *resultp)
{
        uint8_t v;
        size_t sz = sizeof(v);

        if (ep != NULL && sz > ep - *pp) {
                return EINVAL;
        }
        memcpy(&v, *pp, sz);
        *pp += sz;
        *resultp = v;
        return 0;
}

int
read_u32(const uint8_t **pp, const uint8_t *ep, uint32_t *resultp)
{
        uint32_t v;
        size_t sz = sizeof(v);

        if (ep != NULL && sz > ep - *pp) {
                return EINVAL;
        }
        memcpy(&v, *pp, sz);
        *pp += sz;
        *resultp = le32_to_host(v);
        return 0;
}

int
read_u64(const uint8_t **pp, const uint8_t *ep, uint64_t *resultp)
{
        uint64_t v;
        size_t sz = sizeof(v);

        if (ep != NULL && sz > ep - *pp) {
                return EINVAL;
        }
        memcpy(&v, *pp, sz);
        *pp += sz;
        *resultp = le64_to_host(v);
        return 0;
}

int
read_vec_count(const uint8_t **pp, const uint8_t *ep, uint32_t *resultp)
{
        const uint8_t *p = *pp;
        uint32_t u32;
        int ret;

        ret = read_leb_u32(&p, ep, &u32);
        if (ret != 0) {
                goto fail;
        }

        // xlog_printf("vec count %" PRIu32 "\n", u32);
        *pp = p;
        *resultp = u32;
        return 0;
fail:
        return ret;
}

int
read_vec_u32(struct mem_context *mctx, const uint8_t **pp, const uint8_t *ep,
             uint32_t *countp, uint32_t **resultp)
{
        return read_vec(mctx, pp, ep, sizeof(uint32_t),
                        (read_elem_func_t)read_leb_u32, countp,
                        (void *)resultp);
}

/*
 * read_vec_with_ctx is similar to read_vec
 * but it passes extra info to read_elem callback:
 * - the array index
 * - the ctx pointer
 */
int
_read_vec_with_ctx_impl(struct mem_context *mctx, const uint8_t **pp,
                        const uint8_t *ep, size_t elem_size,
                        int (*read_elem)(const uint8_t **pp, const uint8_t *ep,
                                         uint32_t idx, void *elem, void *),
                        void (*clear_elem)(struct mem_context *mctx,
                                           void *elem),
                        void *ctx, uint32_t *countp, void **resultp)
{
        const uint8_t *p = *pp;
        uint32_t vec_count;
        int ret;

        uint32_t orig_count = *countp;
        assert((orig_count == 0) == (*resultp == NULL));
        ret = read_vec_count(&p, ep, &vec_count);
        if (ret != 0) {
                goto fail;
        }
        if (vec_count == 0) {
                *pp = p;
                return 0;
        }
        uint32_t total_count = orig_count + vec_count;
        ret = array_extend(mctx, resultp, elem_size, orig_count, total_count);
        if (ret != 0) {
                goto fail;
        }
        uint8_t *a = (uint8_t *)(*resultp) + orig_count * elem_size;
        uint32_t i;
        for (i = 0; i < vec_count; i++) {
                ret = read_elem(&p, ep, i, a + i * elem_size, ctx);
                if (ret != 0) {
                        if (clear_elem != NULL) {
                                uint32_t j;
                                for (j = 0; j < i; j++) {
                                        clear_elem(mctx, a + j * elem_size);
                                }
                        }
                        /* revert the array size */
                        assert(orig_count < total_count);
                        int ret1 = array_shrink(mctx, resultp, elem_size,
                                                total_count, orig_count);
                        assert(ret1 == 0);
                        goto fail;
                }
        }

        *pp = p;
        *countp = total_count;
        return 0;
fail:
        return ret;
}

struct read_vec_ctx {
        int (*read_elem)(const uint8_t **pp, const uint8_t *ep, void *elem);
        void *ctx;
};

static int
read_elem_wrapper(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
                  void *elem, void *vp)
{
        struct read_vec_ctx *ctx = vp;
        return ctx->read_elem(pp, ep, elem);
}

int
read_vec(struct mem_context *mctx, const uint8_t **pp, const uint8_t *ep,
         size_t elem_size,
         int (*read_elem)(const uint8_t **pp, const uint8_t *ep, void *elem),
         uint32_t *countp, void **resultp)
{
        struct read_vec_ctx ctx = {
                .read_elem = read_elem,
        };
        return _read_vec_with_ctx_impl(mctx, pp, ep, elem_size,
                                       read_elem_wrapper, NULL, &ctx, countp,
                                       resultp);
}
