/*
 * a simple expr parsing api.
 *
 * a wasm expression (expr) is a sequence of instructions
 * terminated by "end".
 *
 * basically this api just provides a way for applications to find out
 * instruction boundaries and the end of the expr.
 *
 * it's assumed that the expr has already been validated.
 * (typically by module_create.)
 *
 * it's up to the applications to parse each instruction if necessary.
 */

#include "platform.h"

struct parse_expr_context {
        uint32_t block_level;
};

__BEGIN_EXTERN_C

void parse_expr_context_init(struct parse_expr_context *pctx);
void parse_expr_context_clear(struct parse_expr_context *pctx);

/*
 * parse_expr: move the pointer to the next instruction.
 *
 * update *pp to the start of the next instruction.
 * set it to NULL when we passed the end of the expr.
 */
void parse_expr(const uint8_t **pp, struct parse_expr_context *pctx);

__END_EXTERN_C
