#include "cell.h"
#include "exec_context.h"
#include "type.h"

#if defined(TOYWASM_CELL_INLINE)
#define TOYWASM_INLINE static inline
#else
#define TOYWASM_INLINE
#endif

TOYWASM_INLINE uint32_t
valtype_cellsize(enum valtype t)
{
#if defined(TOYWASM_USE_SMALL_CELLS)
        uint32_t sz;
        switch (t) {
        case TYPE_i32:
        case TYPE_f32:
                sz = 1;
                break;
        case TYPE_i64:
        case TYPE_f64:
                sz = 2;
                break;
#if defined(TOYWASM_ENABLE_WASM_SIMD)
        case TYPE_v128:
                sz = 4;
                break;
#endif
        case TYPE_funcref:
        case TYPE_externref:
                sz = EXTERNREF_NCELLS;
                assert(sizeof(void *) == sz * sizeof(struct cell));
                break;
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        case TYPE_exnref:
                sz = EXNREF_NCELLS;
                assert(sizeof(struct wasm_exception) <=
                       sz * sizeof(struct cell));
                break;
#endif
        default:
                xassert(false);
                sz = 0;
                break;
        }
        return sz;
#else
        return 1;
#endif
}

TOYWASM_INLINE void
cells_copy(struct cell *restrict dst, const struct cell *restrict src,
           uint32_t ncells)
{
#if 0
        /*
         * ncells is usually 1 or 2 here.
         * too much unrolling hurts.
         */
#pragma clang loop unroll_count(2)
        while (ncells > 0) {
                *dst++ = *src++;
                ncells--;
        }
#else
#if defined(__has_builtin)
#if __has_builtin(__builtin_memcpy_inline)
        if (__predict_true(ncells == 1)) {
                __builtin_memcpy_inline(dst, src, 1 * sizeof(*dst));
                return;
        } else if (__predict_true(ncells == 2)) {
                __builtin_memcpy_inline(dst, src, 2 * sizeof(*dst));
                return;
        }
#endif
#endif
        memcpy(dst, src, ncells * sizeof(*dst));
#endif
}

TOYWASM_INLINE void
val_to_cells(const struct val *val, struct cell *cells, uint32_t ncells)
{
        assert(ncells <= ARRAYCOUNT(val->u.cells));
        cells_copy(cells, val->u.cells, ncells);
}

TOYWASM_INLINE void
val_from_cells(struct val *val, const struct cell *cells, uint32_t ncells)
{
        assert(ncells <= ARRAYCOUNT(val->u.cells));
        cells_copy(val->u.cells, cells, ncells);
}

TOYWASM_INLINE uint32_t
cellidx_lookup(const uint16_t *p, uint32_t idx, uint32_t *cszp)
{
        p += idx;
        uint16_t cidx = *p;
        if (cszp != NULL) {
                uint16_t next_cidx = p[1];
                *cszp = next_cidx - cidx;
        }
        return cidx;
}

#if defined(TOYWASM_USE_LOCALS_FAST_PATH)
TOYWASM_INLINE uint32_t
frame_locals_cellidx_fast(struct exec_context *ctx, uint32_t localidx,
                          uint32_t *cszp)
{
        xassert(cszp != NULL);
        const struct local_info_fast *fast = &ctx->local_u.fast;
        xassert(fast->paramtype_cellidxes != NULL);
        xassert(fast->localtype_cellidxes != NULL);
        uint32_t cidx;
        uint32_t nparams = fast->nparams;
        if (localidx < nparams) {
                cidx = cellidx_lookup(fast->paramtype_cellidxes, localidx,
                                      cszp);
        } else {
                cidx = fast->paramcsz;
                cidx += cellidx_lookup(fast->localtype_cellidxes,
                                       localidx - nparams, cszp);
        }
        return cidx;
}
#endif

/*
 * frame_locals_cellidx: calculate the index and size of a local
 * for the given localidx
 *
 * as this is called on every `local.get`, it is one of
 * the most performance critical code in the interpreter.
 */
TOYWASM_INLINE uint32_t
frame_locals_cellidx(struct exec_context *ctx, uint32_t localidx,
                     uint32_t *cszp)
{
        xassert(cszp != NULL);
#if defined(TOYWASM_USE_SMALL_CELLS)
#if defined(TOYWASM_USE_LOCALS_FAST_PATH)
        if (__predict_true(ctx->fast)) {
                return frame_locals_cellidx_fast(ctx, localidx, cszp);
        }
#endif
        uint32_t frame_locals_cellidx_slow(struct exec_context * ctx,
                                           uint32_t localidx, uint32_t *cszp);
        return frame_locals_cellidx_slow(ctx, localidx, cszp);
#else  /* defined(TOYWASM_USE_SMALL_CELLS) */
        *cszp = 1;
        return localidx;
#endif /* defined(TOYWASM_USE_SMALL_CELLS) */
}

#undef TOYWASM_INLINE
