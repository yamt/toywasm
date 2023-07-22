#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "cell.h"
#include "exec_context.h"
#include "type.h"

uint32_t
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
        case TYPE_FUNCREF:
        case TYPE_EXTERNREF:
                sz = sizeof(void *) / sizeof(struct cell);
                assert(sizeof(void *) == sz * sizeof(struct cell));
                break;
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

#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX) ||                                \
        defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
static uint32_t
localcellidx_lookup(const struct localcellidx *lci, uint32_t idx,
                    uint32_t *cszp)
{
        const uint16_t *p = &lci->cellidxes[idx];
        uint16_t cidx = *p;
        if (cszp != NULL) {
                uint16_t next_cidx = p[1];
                *cszp = next_cidx - cidx;
        }
        return cidx;
}
#endif

uint32_t
resulttype_cellidx(const struct resulttype *rt, uint32_t idx, uint32_t *cszp)
{
#if defined(TOYWASM_USE_SMALL_CELLS)
        assert(idx < rt->ntypes || (idx == rt->ntypes && cszp == NULL));
#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
        if (__predict_true(rt->cellidx.cellidxes != NULL)) {
                return localcellidx_lookup(&rt->cellidx, idx, cszp);
        }
#endif
        /* REVISIT: very inefficient */
        uint32_t sz = 0;
        uint32_t i;
        for (i = 0; i < idx; i++) {
                uint32_t esz = valtype_cellsize(rt->types[i]);
                assert(UINT32_MAX - sz >= esz);
                sz += esz;
        }
        if (cszp != NULL) {
                *cszp = valtype_cellsize(rt->types[idx]);
        }
        return sz;
#else
        if (cszp != NULL) {
                *cszp = 1;
        }
        return idx;
#endif
}

uint32_t
resulttype_cellsize(const struct resulttype *rt)
{
        return resulttype_cellidx(rt, rt->ntypes, NULL);
}

static uint32_t
localchunk_cellidx(const struct localchunk *localchunks, uint32_t localidx,
                   uint32_t *cszp)
{
#if defined(TOYWASM_USE_SMALL_CELLS)
        /* REVISIT: very inefficient */
        const struct localchunk *chunk = localchunks;
        uint32_t cellidx = 0;
        while (localidx > 0) {
                uint32_t n = chunk->n;
                if (n > localidx) {
                        n = localidx;
                }
                uint32_t vsz = valtype_cellsize(chunk->type);
                uint32_t csz = n * vsz;
                assert(csz / n == vsz);
                assert(UINT32_MAX - cellidx >= csz);
                cellidx += csz;
                localidx -= n;
                if (n == chunk->n) {
                        chunk++;
                }
        }
        if (cszp != NULL) {
                *cszp = valtype_cellsize(chunk->type);
        }
        return cellidx;
#else
        if (cszp != NULL) {
                *cszp = 1;
        }
        return localidx;
#endif
}

uint32_t
localtype_cellidx(const struct localtype *lt, uint32_t idx, uint32_t *cszp)
{
        assert(idx < lt->nlocals || (idx == lt->nlocals && cszp == NULL));
#if defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
        if (__predict_true(lt->cellidx.cellidxes != NULL)) {
                return localcellidx_lookup(&lt->cellidx, idx, cszp);
        }
#endif
        return localchunk_cellidx(lt->localchunks, idx, cszp);
}

uint32_t
localtype_cellsize(const struct localtype *lt)
{
        return localtype_cellidx(lt, lt->nlocals, NULL);
}

/*
 * frame_locals_cellidx: calculate the index and size of a local
 * for the given localidx
 *
 * as this is called on every `local.get`, it is one of
 * the most performance critical code in the interpreter.
 */
uint32_t
frame_locals_cellidx(struct exec_context *ctx, uint32_t localidx,
                     uint32_t *cszp)
{
        xassert(cszp != NULL);
#if defined(TOYWASM_USE_SMALL_CELLS)
        uint32_t cidx;
        uint32_t nparams = ctx->paramtype->ntypes;
        if (localidx < nparams) {
                cidx = resulttype_cellidx(ctx->paramtype, localidx, cszp);
        } else {
                assert(localidx < nparams + ctx->localtype->nlocals);
                cidx = resulttype_cellsize(ctx->paramtype);
                cidx += localtype_cellidx(ctx->localtype, localidx - nparams,
                                          cszp);
        }
        return cidx;
#else
        *cszp = 1;
        return localidx;
#endif
}

void
val_to_cells(const struct val *val, struct cell *cells, uint32_t ncells)
{
#if defined(TOYWASM_ENABLE_WASM_SIMD)
        assert(ncells == 1 || ncells == 2 || ncells == 4 || ncells == 8);
#else
        assert(ncells == 1 || ncells == 2 || ncells == 4);
#endif
        cells_copy(cells, val->u.cells, ncells);
}

void
val_from_cells(struct val *val, const struct cell *cells, uint32_t ncells)
{
#if defined(TOYWASM_ENABLE_WASM_SIMD)
        assert(ncells == 1 || ncells == 2 || ncells == 4 || ncells == 8);
#else
        assert(ncells == 1 || ncells == 2 || ncells == 4);
#endif
        cells_copy(val->u.cells, cells, ncells);
}

void
vals_to_cells(const struct val *vals, struct cell *cells,
              const struct resulttype *rt)
{
        uint32_t n = rt->ntypes;
        uint32_t i;
        for (i = 0; i < n; i++) {
                uint32_t csz = valtype_cellsize(rt->types[i]);
                val_to_cells(vals, cells, csz);
                vals++;
                cells += csz;
        }
}

void
vals_from_cells(struct val *vals, const struct cell *cells,
                const struct resulttype *rt)
{
        uint32_t n = rt->ntypes;
        uint32_t i;
        for (i = 0; i < n; i++) {
                uint32_t csz = valtype_cellsize(rt->types[i]);
                val_from_cells(vals, cells, csz);
                vals++;
                cells += csz;
        }
}

void
cells_zero(struct cell *cells, uint32_t ncells)
{
        memset(cells, 0, sizeof(*cells) * ncells);
}

void
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

void
cells_move(struct cell *dst, const struct cell *src, uint32_t ncells)
{
        if (dst <= src) {
                while (ncells > 0) {
                        *dst++ = *src++;
                        ncells--;
                }
        } else {
                dst += ncells - 1;
                src += ncells - 1;
                while (ncells > 0) {
                        *dst-- = *src--;
                        ncells--;
                }
        }
}
