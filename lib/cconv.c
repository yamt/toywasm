/*
 * this file implements C conventions, which is not necessarily
 * a part of core wasm.
 */

#include "cconv.h"
#include "exec.h"
#include "instance.h"
#include "module.h"
#include "type.h"

static const struct name name_func_table =
        NAME_FROM_CSTR_LITERAL("__indirect_function_table");

/*
 * the export name "memory" is defined by wasi.
 * https://github.com/WebAssembly/WASI/blob/main/legacy/README.md
 * it's widely used for non-wasi interfaces as well.
 *
 * Note: some runtimes, including old versions of toywasm, assume
 * the memidx 0 even for wasi. it would end up with some
 * incompatibilities with multi-memory proposal.
 */
static const struct name name_default_memory =
        NAME_FROM_CSTR_LITERAL("memory");

/*
 * dereference a C function pointer.
 * that is, get the func inst pointed by the pointer.
 * raise a trap on an error.
 */
int
cconv_deref_func_ptr(struct exec_context *ctx, const struct instance *inst,
                     uint32_t wasmfuncptr, const struct functype *ft,
                     const struct funcinst **fip)
{
        const struct module *m = inst->module;
        uint32_t tableidx;
        int ret;
        /*
         * XXX searching exports on each call can be too slow.
         */
        ret = module_find_export(m, &name_func_table, EXTERNTYPE_TABLE,
                                 &tableidx);
        if (ret != 0) {
do_trap:
                return trap_with_id(ctx,
                                    TRAP_INDIRECT_FUNCTION_TABLE_NOT_FOUND,
                                    "__indirect_function_table is not found");
        }
        const struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
        if (t->type->et != TYPE_funcref) {
                goto do_trap;
        }
        const struct funcinst *func;
        ret = table_get_func(ctx, t, wasmfuncptr, ft, &func);
        if (ret != 0) {
                /* Note: table_get_func raises a trap inside */
                return ret;
        }
        *fip = func;
        return 0;
}

int
cconv_default_memory(struct exec_context *ctx, uint32_t *memidxp)
{
        const struct module *m = ctx->instance->module;
        int ret;
        /*
         * XXX searching exports on each call can be too slow.
         */
        ret = module_find_export(m, &name_default_memory, EXTERNTYPE_MEMORY,
                                 memidxp);
        if (ret != 0) {
                return trap_with_id(ctx, TRAP_DEFAULT_MEMORY_NOT_FOUND,
                                    "default memory not found");
        }
        return 0;
}
