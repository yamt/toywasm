
#include <stdbool.h>
#include <stdint.h>

struct expr;
struct exec_context;
struct funcinst;
struct resulttype;
struct val;
enum valtype;

int exec_expr(uint32_t funcidx, const struct expr *expr,
              const struct localtype *localtype,
              const struct resulttype *paramtype, uint32_t nresults,
              const struct cell *params, struct cell *results,
              struct exec_context *ctx);

int exec_const_expr(const struct expr *expr, enum valtype type,
                    struct val *result, struct exec_context *ctx);

int memory_init(struct exec_context *ctx, uint32_t memidx, uint32_t dataidx,
                uint32_t d, uint32_t s, uint32_t n);
uint32_t memory_grow(struct exec_context *ctx, uint32_t memidx,
                     uint32_t newsize);
int table_init(struct exec_context *ctx, uint32_t tableidx, uint32_t elemidx,
               uint32_t d, uint32_t s, uint32_t n);
int table_access(struct exec_context *ectx, uint32_t tableidx, uint32_t offset,
                 uint32_t n);
void data_drop(struct exec_context *ectx, uint32_t dataidx);
void elem_drop(struct exec_context *ectx, uint32_t elemidx);

bool skip_expr(const uint8_t **p, bool goto_else);
int exec_next_insn(const uint8_t *p, struct cell *stack,
                   struct exec_context *ctx);
void rewind_stack(struct exec_context *ctx, uint32_t height, uint32_t arity);

int invoke(struct funcinst *finst, const struct resulttype *paramtype,
           const struct resulttype *resulttype, const struct cell *params,
           struct cell *results, struct exec_context *ctx);
