#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "cell.h"
#include "context.h"
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
        case TYPE_FUNCREF:
        case TYPE_EXTERNREF:
                sz = sizeof(void *) / sizeof(struct cell);
                assert(sizeof(void *) == sz * sizeof(struct cell));
                break;
        default:
                assert(false);
                sz = 0;
                break;
        }
        return sz;
#else
        return 1;
#endif
}

uint32_t
resulttype_cellidx(const struct resulttype *rt, uint32_t idx, uint32_t *cszp)
{
#if defined(TOYWASM_USE_SMALL_CELLS)
        /* REVISIT: very inefficient */
        assert(idx < rt->ntypes || (idx == rt->ntypes && cszp == NULL));
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
        return localchunk_cellidx(lt->localchunks, idx, cszp);
}

uint32_t
localtype_cellsize(const struct localtype *lt)
{
        return localtype_cellidx(lt, lt->nlocals, NULL);
}

uint32_t
frame_locals_cellidx(struct exec_context *ctx, uint32_t localidx,
                     uint32_t *cszp)
{
#if defined(TOYWASM_USE_SMALL_CELLS)
        /* REVISIT: very inefficient */
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
        size_t sz = ncells * sizeof(*cells);
        assert(sizeof(*val) >= sz);
        memcpy(cells, val, sz);
}

void
val_from_cells(struct val *val, const struct cell *cells, uint32_t ncells)
{
        size_t sz = ncells * sizeof(*cells);
        assert(sizeof(*val) >= sz);
        memcpy(val, cells, sz);
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
        memset(cells, 0, ncells * sizeof(*cells));
}

void
cells_copy(struct cell *dst, const struct cell *src, uint32_t ncells)
{
        memcpy(dst, src, ncells * sizeof(*dst));
}

void
cells_move(struct cell *dst, const struct cell *src, uint32_t ncells)
{
        memmove(dst, src, ncells * sizeof(*dst));
}
