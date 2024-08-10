/*
 * this file implements C conventions, which is not necessarily
 * a part of core wasm.
 */

#include <assert.h>

#include "cconv.h"
#include "exec.h"
#include "instance.h"
#include "module.h"
#include "type.h"

/*
 * the export name "memory" and "__indirect_function_table" are
 * defined by wasi.
 * https://github.com/WebAssembly/WASI/blob/main/legacy/README.md
 * they are widely used for non-wasi interfaces as well.
 *
 * Note: some runtimes, including old versions of toywasm, assume
 * the memidx 0 even for wasi. it would end up with some
 * incompatibilities with multi-memory proposal.
 */

static const struct name name_func_table =
        NAME_FROM_CSTR_LITERAL("__indirect_function_table");
static const struct name name_default_memory =
        NAME_FROM_CSTR_LITERAL("memory");

/*
 * dereference a C function pointer.
 * that is, get the func inst pointed by the pointer.
 * raise a trap on an error.
 */
int
cconv_deref_func_ptr(struct exec_context *ctx, const struct tableinst *t,
                     uint32_t wasmfuncptr, const struct functype *ft,
                     const struct funcinst **fip)
{
        if (t == NULL) {
                return trap_with_id(
                        ctx, TRAP_INDIRECT_FUNCTION_TABLE_NOT_FOUND,
                        "no suitable table for indirect function table");
        }
        assert(t->type->et == TYPE_funcref);
        const struct funcinst *func;
        int ret = table_get_func(ctx, t, wasmfuncptr, ft, &func);
        if (ret != 0) {
                /* Note: table_get_func raises a trap inside */
                return ret;
        }
        *fip = func;
        return 0;
}

struct meminst *
cconv_memory(const struct instance *inst)
{
        const struct module *m = inst->module;
        uint32_t memidx;
        int ret = module_find_export(m, &name_default_memory,
                                     EXTERNTYPE_MEMORY, &memidx);
        if (ret != 0) {
                return NULL;
        }
        return VEC_ELEM(inst->mems, memidx);
}

struct tableinst *
cconv_func_table(const struct instance *inst)
{
        const struct module *m = inst->module;
        uint32_t tableidx;
        int ret = module_find_export(m, &name_func_table, EXTERNTYPE_TABLE,
                                     &tableidx);
        if (ret != 0) {
                return NULL;
        }
        struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
        if (t->type->et != TYPE_funcref) {
                return NULL;
        }
        return t;
}
