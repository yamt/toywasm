#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "platform.h"

int read_u8(const uint8_t **pp, const uint8_t *ep, uint8_t *resultp);
int read_u32(const uint8_t **pp, const uint8_t *ep, uint32_t *resultp);
int read_u64(const uint8_t **pp, const uint8_t *ep, uint64_t *resultp);

int read_vec_count(const uint8_t **pp, const uint8_t *ep, uint32_t *resultp);
int read_vec_u32(const uint8_t **pp, const uint8_t *ep, uint32_t *countp,
                 uint32_t **resultp);

int read_vec(const uint8_t **pp, const uint8_t *ep, size_t elem_size,
             int (*read_elem)(const uint8_t **pp, const uint8_t *ep,
                              void *elem),
             void (*clear_elem)(void *elem), uint32_t *countp, void **resultp);

int _read_vec_with_ctx_impl(const uint8_t **pp, const uint8_t *ep,
                            size_t elem_size,
                            int (*read_elem)(const uint8_t **pp,
                                             const uint8_t *ep, uint32_t idx,
                                             void *elem, void *ctx),
                            void (*clear_elem)(void *elem), void *ctx,
                            uint32_t *countp, void **resultp);

#if defined(toywasm_typeof)
/* a version with non-standard checks */
#define read_vec_with_ctx(pp, ep, elem_size, read_elem, clear_elem, ctx,      \
                          countp, resultp)                                    \
        ({                                                                    \
                int (*_r)(const uint8_t **, const uint8_t *, uint32_t,        \
                          toywasm_typeof(*resultp), void *) = read_elem;      \
                void (*_c)(toywasm_typeof(*resultp)) = clear_elem;            \
                assert(sizeof(**resultp) == elem_size);                       \
                _read_vec_with_ctx_impl(pp, ep, elem_size, (void *)_r,        \
                                        (void *)_c, ctx, countp,              \
                                        (void **)resultp);                    \
        })
#else
#define read_vec_with_ctx(pp, ep, elem_size, read_elem, clear_elem, ctx,      \
                          countp, resultp)                                    \
        _read_vec_with_ctx_impl(pp, ep, elem_size, (void *)read_elem,         \
                                (void *)clear_elem, ctx, countp,              \
                                (void **)resultp)
#endif
