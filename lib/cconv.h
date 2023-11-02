#include <stdint.h>

struct exec_context;
struct instance;
struct functype;
struct funcinst;

int cconv_deref_func_ptr(struct exec_context *ctx, const struct instance *inst,
                         uint32_t wasmfuncptr, const struct functype *ft,
                         const struct funcinst **fip);
