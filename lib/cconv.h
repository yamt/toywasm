#include <stdint.h>

#include "platform.h"

struct exec_context;
struct instance;
struct functype;
struct funcinst;
struct meminst;

__BEGIN_EXTERN_C

int cconv_deref_func_ptr(struct exec_context *ctx, const struct instance *inst,
                         uint32_t wasmfuncptr, const struct functype *ft,
                         const struct funcinst **fip);

struct meminst *cconv_default_memory(const struct instance *inst);

__END_EXTERN_C
