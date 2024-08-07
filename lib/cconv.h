#include <stdint.h>

#include "platform.h"

struct exec_context;
struct instance;
struct functype;
struct funcinst;

__BEGIN_EXTERN_C

int cconv_deref_func_ptr(struct exec_context *ctx, const struct instance *inst,
                         uint32_t wasmfuncptr, const struct functype *ft,
                         const struct funcinst **fip);

int cconv_default_memory(struct exec_context *ctx, const struct instance *inst,
                         uint32_t *memidxp);

__END_EXTERN_C
