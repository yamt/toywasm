
#include <stdbool.h>
#include <stdint.h>

struct expr;
struct context;
struct resulttype;
enum valtype;

int exec_expr(const struct expr *expr, uint32_t nlocals,
              const enum valtype *locals, const struct resulttype *,
              const struct resulttype *resulttype, const struct val *params,
              struct val *results, struct exec_context *ctx);

int exec_const_expr(const struct expr *expr, enum valtype type,
                    struct val *result, struct exec_context *ctx);

int memory_init(struct exec_context *ctx, uint32_t dataidx, uint32_t d,
                uint32_t s, uint32_t n);
uint32_t memory_grow(struct exec_context *ctx, uint32_t memidx,
                     uint32_t newsize);

bool skip_expr(const uint8_t **p, bool goto_else);
int exec_next_insn(const uint8_t **pp, struct exec_context *ctx);
