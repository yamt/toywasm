#if !defined(_TOYWASM_CELL_H)
#define _TOYWASM_CELL_H

#include <stdint.h>

#include "toywasm_config.h"

#include "platform.h"
#include "valtype.h"

struct localtype;
struct resulttype;
struct funcframe;
struct exec_context;
struct val;

/*
 * a cell represents host memory to store a wasm value. (struct val)
 *
 * a value can be represented by multiple consecutive cells, depending on
 * the configuration and the type of the value.
 *
 * cells do not necessarily have the same alignment as the corresponding
 * value. you might need to use memcpy equivalent between them.
 */
struct cell {
#if defined(TOYWASM_USE_SMALL_CELLS)
        uint32_t x;
#else
#if defined(TOYWASM_ENABLE_WASM_SIMD)
        uint64_t x[2];
#else
        uint64_t x;
#endif
#endif
};

__BEGIN_EXTERN_C

uint32_t valtype_cellsize(enum valtype t) __constfunc;

uint32_t resulttype_cellidx(const struct resulttype *rt, uint32_t idx,
                            uint32_t *cszp);
uint32_t resulttype_cellsize(const struct resulttype *rt);

uint32_t localtype_cellidx(const struct localtype *lt, uint32_t idx,
                           uint32_t *cszp);
uint32_t localtype_cellsize(const struct localtype *lt);

uint32_t frame_locals_cellidx(struct exec_context *ctx, uint32_t localidx,
                              uint32_t *cszp);

void val_to_cells(const struct val *val, struct cell *cells, uint32_t ncells);
void val_from_cells(struct val *val, const struct cell *cells,
                    uint32_t ncells);

void vals_to_cells(const struct val *vals, struct cell *cells,
                   const struct resulttype *rt);
void vals_from_cells(struct val *vals, const struct cell *cells,
                     const struct resulttype *rt);

void cells_zero(struct cell *cells, uint32_t ncells);
void cells_copy(struct cell *dst, const struct cell *src, uint32_t ncells);
void cells_move(struct cell *dst, const struct cell *src, uint32_t ncells);

__END_EXTERN_C

#endif /* !defined(_TOYWASM_CELL_H) */
