#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "platform.h"

struct mem_context;

int read_u8(const uint8_t **pp, const uint8_t *ep, uint8_t *resultp);
int read_u32(const uint8_t **pp, const uint8_t *ep, uint32_t *resultp);
int read_u64(const uint8_t **pp, const uint8_t *ep, uint64_t *resultp);

int read_vec_count(const uint8_t **pp, const uint8_t *ep, uint32_t *resultp);
int read_vec_u32(struct mem_context *mctx, const uint8_t **pp,
                 const uint8_t *ep, uint32_t *countp, uint32_t **resultp);

typedef int (*read_elem_func_t)(const uint8_t **pp, const uint8_t *ep,
                                void *elem);
typedef int (*read_elem_with_ctx_func_t)(const uint8_t **pp, const uint8_t *ep,
                                         uint32_t idx, void *elem, void *ctx);
typedef void (*clear_elem_func_t)(void *elem);

int read_vec(struct mem_context *mctx, const uint8_t **pp, const uint8_t *ep,
             size_t elem_size, read_elem_func_t read_elem,
             clear_elem_func_t clear_elem, uint32_t *countp, void **resultp);

int _read_vec_with_ctx_impl(struct mem_context *mctx, const uint8_t **pp,
                            const uint8_t *ep, size_t elem_size,
                            read_elem_with_ctx_func_t read_elem,
                            clear_elem_func_t clear_elem, void *ctx,
                            uint32_t *countp, void **resultp);

/*
 * Note: here we cast functions taking "struct foo *" to function pointers
 * taking "void *" and call them via the pointers. (without casting them
 * back to the correct type.)
 * IIRC, it's an undefined behavior in C.
 * While I'm not entirely happy with this,
 * - It works on most of the C runtime environments, including the C ABI
 *   usually used for WASM.
 * - I prefer to have type checks than using "void *" in every callback
 *   implementations.
 */

#if defined(toywasm_typeof)
/* a version with non-standard checks */
#define read_vec_with_ctx(mctx, pp, ep, elem_size, read_elem, clear_elem,     \
                          ctx, countp, resultp)                               \
        ({                                                                    \
                int (*_r)(const uint8_t **, const uint8_t *, uint32_t,        \
                          toywasm_typeof(*resultp), void *) = read_elem;      \
                void (*_c)(toywasm_typeof(*resultp)) = clear_elem;            \
                assert(sizeof(**resultp) == elem_size);                       \
                _read_vec_with_ctx_impl(mctx, pp, ep, elem_size,              \
                                        (read_elem_with_ctx_func_t)_r,        \
                                        (clear_elem_func_t)_c, ctx, countp,   \
                                        (void **)resultp);                    \
        })
#else
#define read_vec_with_ctx(mctx, pp, ep, elem_size, read_elem, clear_elem,     \
                          ctx, countp, resultp)                               \
        _read_vec_with_ctx_impl(mctx, pp, ep, elem_size,                      \
                                (read_elem_with_ctx_func_t)read_elem,         \
                                (clear_elem_func_t)clear_elem, ctx, countp,   \
                                (void **)resultp)
#endif

struct name;
int read_name(const uint8_t **pp, const uint8_t *ep, struct name *namep);
