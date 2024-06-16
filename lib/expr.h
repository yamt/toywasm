#include <stdint.h>

#include "valtype.h"

struct expr;
struct resulttype;
struct localchunk;
struct load_context;
struct mem_context;
struct module;

int get_functype_for_blocktype(struct mem_context *mctx, struct module *m,
                               int64_t blocktype,
                               struct resulttype **parameter,
                               struct resulttype **result);

int read_expr(const uint8_t **pp, const uint8_t *ep, struct expr *expr,
              uint32_t nlocals, const struct localchunk *localchunks,
              struct resulttype *, struct resulttype *,
              struct load_context *lctx);
int read_const_expr(const uint8_t **pp, const uint8_t *ep, struct expr *expr,
                    enum valtype type, struct load_context *lctx);
