/*
 * this file implements C conventions, which is not necessarily
 * a part of core wasm.
 */

#include "exec.h"
#include "module.h"
#include "type.h"

static const struct name name_func_table =
        NAME_FROM_CSTR_LITERAL("__indirect_function_table");

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
        if (t->type->et != TYPE_FUNCREF) {
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
