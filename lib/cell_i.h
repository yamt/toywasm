#include "cell.h"
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

#undef TOYWASM_INLINE
