#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "context.h"
#include "expr_parser.h"
#include "insn.h"
#include "leb128.h"

void
parse_expr_context_init(struct parse_expr_context *pctx)
{
        pctx->block_level = 0;
}

void
parse_expr_context_clear(struct parse_expr_context *pctx)
{
}

void
parse_expr(const uint8_t **pp, struct parse_expr_context *pctx)
{
        const uint8_t *p = *pp;
        uint32_t op = read_insn(&p);
        switch (op) {
        case FRAME_OP_BLOCK:
        case FRAME_OP_LOOP:
        case FRAME_OP_IF:
        case FRAME_OP_TRY_TABLE:
                pctx->block_level++;
                break;
        case FRAME_OP_END:
                if (pctx->block_level == 0) {
                        /* the end of the exprs */
                        *pp = NULL;
                        return;
                }
                pctx->block_level--;
                break;
        default:
                break;
        }
        *pp = p;
}
