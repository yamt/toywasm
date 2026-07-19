#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "cell.h"
#include "exec_context.h"
#include "type.h"

/* provide non-inline version */
#include "cell_i.h"

#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX) ||                                \
        defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
static uint32_t
localcellidx_lookup(const struct localcellidx *lci, uint32_t idx,
                    uint32_t *cszp)
{
        return cellidx_lookup(lci->cellidxes, idx, cszp);
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

#if defined(TOYWASM_USE_SMALL_CELLS)
#if 0
static __noinline
#endif
uint32_t
frame_locals_cellidx_slow(struct exec_context *ctx, uint32_t localidx,
                          uint32_t *cszp)
{
        xassert(cszp != NULL);
        const struct local_info_slow *slow = &ctx->local_u.slow;
        uint32_t cidx;
        uint32_t nparams = slow->paramtype->ntypes;
        if (localidx < nparams) {
                cidx = resulttype_cellidx(slow->paramtype, localidx, cszp);
        } else {
                assert(localidx < nparams + slow->localtype->nlocals);
                cidx = resulttype_cellsize(slow->paramtype);
                cidx += localtype_cellidx(slow->localtype, localidx - nparams,
                                          cszp);
        }
        return cidx;
}
#endif /* defined(TOYWASM_USE_SMALL_CELLS) */

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
