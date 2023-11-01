
#include <errno.h>
#include <stdlib.h>

#include <toywasm/endian.h>
#include <toywasm/host_instance.h>

static int
my_host_inst_load(struct exec_context *ctx, struct host_instance *hi,
                  const struct functype *ft, const struct cell *params,
                  struct cell *results)
{
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t pp = HOST_FUNC_PARAM(ft, params, 0, i32);
        int host_ret;
        uint32_t le32;

        host_ret = host_func_copyin(ctx, &le32, pp, 4, 4);
        if (host_ret != 0) {
                goto fail;
        }
        uint32_t p = le32_to_host(le32);
        host_ret = host_func_copyin(ctx, &le32, p, 4, 4);
        if (host_ret != 0) {
                goto fail;
        }
        uint32_t result = le32_to_host(le32);
        p += 4;
        le32 = host_to_le32(p);
        host_ret = host_func_copyout(ctx, &le32, pp, 4, 4);
        if (host_ret != 0) {
                goto fail;
        }
fail:
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32, result);
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static const struct host_func my_host_inst_funcs[] = {
        HOST_FUNC(my_host_inst_, load, "(i)i"),
};

static const struct name name_my_host_inst =
        NAME_FROM_CSTR_LITERAL("my-host-func");

static const struct host_module module_my_host_inst[] = {{
        .module_name = &name_my_host_inst,
        .funcs = my_host_inst_funcs,
        .nfuncs = ARRAYCOUNT(my_host_inst_funcs),
}};

int
import_object_create_for_my_host_inst(void *inst, struct import_object **impp)
{
        return import_object_create_for_host_funcs(
                module_my_host_inst, ARRAYCOUNT(module_my_host_inst), inst,
                impp);
}
