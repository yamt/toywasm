/* wasm-opt --log-execution=env */

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <toywasm/context.h>
#include <toywasm/exec_context.h>
#include <toywasm/exec_debug.h>
#include <toywasm/host_instance.h>
#include <toywasm/name.h>
#include <toywasm/nbio.h>

static int
log_execution(struct exec_context *ctx, struct host_instance *hi,
              const struct functype *ft, const struct cell *params,
              struct cell *results)
{
        struct nametable *table = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t i = HOST_FUNC_PARAM(ft, params, 0, i32);
        const struct module *m = ctx->instance->module;
        uint32_t pc = ptr2pc(m, ctx->p);
        uint32_t funcidx = VEC_LASTELEM(ctx->frames).funcidx;
        struct name func_name;
        nametable_lookup_func(table, m, funcidx, &func_name);
        struct name module_name;
        nametable_lookup_module(table, m, &module_name);
        nbio_printf("log-execution idx=%08" PRIu32 " callerpc=%06" PRIx32
                    " (%.*s:%.*s)\n",
                    i, pc, CSTR(&module_name), CSTR(&func_name));
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}

static const struct host_func log_execution_funcs[] = {
        HOST_FUNC("log_execution", log_execution, "(i)"),
};

static const struct name name_log_execution = NAME_FROM_CSTR_LITERAL("env");

static const struct host_module module_log_execution[] = {{
        .module_name = &name_log_execution,
        .funcs = log_execution_funcs,
        .nfuncs = ARRAYCOUNT(log_execution_funcs),
}};

int
import_object_create_for_log_execution(void *inst, struct import_object **impp)
{
        return import_object_create_for_host_funcs(
                module_log_execution, ARRAYCOUNT(module_log_execution), inst,
                impp);
}
