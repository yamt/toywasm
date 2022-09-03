#if !defined(_CELL_H)
#define _CELL_H

#include <stdint.h>

enum valtype;
struct localtype;
struct resulttype;
struct funcframe;
struct exec_context;
struct val;

struct cell {
#if defined(USE_SMALL_CELLS)
        uint32_t x;
#else
        uint64_t x;
#endif
};

uint32_t valtype_cellsize(enum valtype t);

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

#endif /* !defined(_CELL_H) */
