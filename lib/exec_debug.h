#include <stdint.h>

#include "platform.h"

struct exec_context;
struct instance;

__BEGIN_EXTERN_C

void print_trace(const struct exec_context *ctx);
void print_memory(const struct exec_context *ctx, const struct instance *inst,
                  uint32_t memidx, uint32_t addr, uint32_t count);
void print_pc(const struct exec_context *ctx);

__END_EXTERN_C
