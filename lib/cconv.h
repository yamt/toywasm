#include <stdint.h>

#include "platform.h"

struct exec_context;
struct instance;
struct functype;
struct funcinst;
struct meminst;
struct tableinst;

__BEGIN_EXTERN_C

int cconv_deref_func_ptr(struct exec_context *ctx, const struct tableinst *t,
                         uint32_t wasmfuncptr, const struct functype *ft,
                         const struct funcinst **fip);

struct meminst *cconv_memory(const struct instance *inst);
struct tableinst *cconv_func_table(const struct instance *inst);

__END_EXTERN_C
