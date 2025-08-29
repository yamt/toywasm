#include <stdint.h>

#include "platform.h"

struct exec_context;
struct instance;
struct functype;
struct funcinst;
struct meminst;
struct tableinst;

__BEGIN_EXTERN_C

/*
 * cconv_deref_func_ptr: dereference the specified C function pointer.
 *
 * on success, return the corresponding funcinst via fip.
 * otherwise, raise a trap. (type mismatch, NULL dereference, and so on.)
 *
 * note: an embedder can use cconv_func_table() to find the appropriate
 * table instance.
 */
int cconv_deref_func_ptr(struct exec_context *ctx, const struct tableinst *t,
                         uint32_t wasmfuncptr, const struct functype *ft,
                         const struct funcinst **fip);

/*
 * cconv_memory: returns the default memory instance.
 *
 * appropriate for wasip1 and similar abi.
 */
struct meminst *cconv_memory(const struct instance *inst);

/*
 * cconv_func_table: returns the default table instance used for
 * C function pointers.
 */
struct tableinst *cconv_func_table(const struct instance *inst);

__END_EXTERN_C
