#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include <toywasm/cconv.h>
#include <toywasm/cell.h>
#include <toywasm/endian.h>
#include <toywasm/exec_context.h>
#include <toywasm/host_instance.h>
#include <toywasm/restart.h>

static int
load(struct exec_context *ctx, uint32_t pp, uint32_t *resultp)
{
        int host_ret;
        uint32_t le32;

        /*
         * *resultp = *(*pp)++
         */

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
        *resultp = result;
fail:
        return host_ret;
}

static int
load_func(struct exec_context *ctx, const struct functype *ft, uint32_t pp,
          const struct funcinst **fip)
{
        int host_ret;
        uint32_t funcptr;
        host_ret = load(ctx, pp, &funcptr);
        if (host_ret != 0) {
                goto fail;
        }
        const struct funcinst *func;
        host_ret =
                cconv_deref_func_ptr(ctx, ctx->instance, funcptr, ft, &func);
        if (host_ret != 0) {
                goto fail;
        }
        *fip = func;
fail:
        return host_ret;
}

static int
my_host_inst_load(struct exec_context *ctx, struct host_instance *hi,
                  const struct functype *ft, const struct cell *params,
                  struct cell *results)
{
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t pp = HOST_FUNC_PARAM(ft, params, 0, i32);
        int host_ret;

        uint32_t result;
        host_ret = load(ctx, pp, &result);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32, result);
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static int
my_host_inst_load_call(struct exec_context *ctx, struct host_instance *hi,
                       const struct functype *ft, const struct cell *params,
                       struct cell *results)
{
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t pp = HOST_FUNC_PARAM(ft, params, 0, i32);
        int host_ret;

        const struct funcinst *func;
        host_ret = load_func(ctx, ft, pp, &func);
        if (host_ret != 0) {
                goto fail;
        }
        /* tail call with the same argument */
        ctx->event_u.call.func = func;
        ctx->event = EXEC_EVENT_CALL;
        host_ret = ETOYWASMRESTART;
fail:
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static int
my_host_inst_load_call_add(struct exec_context *ctx, struct host_instance *hi,
                           const struct functype *ft,
                           const struct cell *params, struct cell *results)
{
        /*
         * this function is a bit complicated as it calls other functions.
         * the callee functions can be wasm functions or host functions.
         *
         *    sum = 0;
         *    f1 = load_func(pp);
         *    v1 = f1(pp); // this might need a restart
         * step1:
         *    sum += v1;
         *    f2 = load_func(pp);
         *    v2 = f2(pp); // this might need a restart
         * step2:
         *    sum += v2;
         *    return sum;
         */

        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t pp = HOST_FUNC_PARAM(ft, params, 0, i32);
        int host_ret;
        struct restart_hostfunc *hf;
        uint32_t result;
        uint32_t sum = 0;
        uint32_t i;

        host_ret = restart_info_prealloc(ctx);
        if (host_ret != 0) {
                return host_ret;
        }
        struct restart_info *restart = &VEC_NEXTELEM(ctx->restarts);
        if (restart->restart_type != RESTART_NONE) {
                assert(restart->restart_type == RESTART_HOSTFUNC);
                hf = &restart->restart_u.hostfunc;
                uint32_t step = hf->user1;
                sum = hf->user2;
                restart_info_clear(ctx);
                switch (step) {
                case 1:
                case 2:
                        assert(ctx->stack.psize - ctx->stack.lsize >=
                               hf->stack_adj);
                        i = step - 1;
                        /*
                         * adjust the stack offset to make exec_pop_vals
                         * below pop the correct values.
                         */
                        ctx->stack.lsize += hf->stack_adj;
                        goto after_return;
                default:
                        assert(false);
                }
                assert(false);
        }
        sum = 0;
        for (i = 0; i < 2; i++) {
                /*
                 * Note: we know the function has the same type as
                 * ours. (ft)
                 */
                const struct funcinst *func;
                host_ret = load_func(ctx, ft, pp, &func);
                if (host_ret != 0) {
                        goto fail;
                }

                /*
                 * call the function
                 */
                struct val a[1] = {
                        {
                                .u.i32 = pp,
                        },
                };
                host_ret = exec_push_vals(ctx, &ft->parameter, a);
                if (host_ret != 0) {
                        goto fail;
                }
                /*
                 * set up the restart info so that the function can
                 * return to us.
                 */
                host_ret = schedule_call_from_hostfunc(ctx, restart, func);
                /* save extra context */
                hf = &restart->restart_u.hostfunc;
                hf->user1 = i + 1; /* step */
                hf->user2 = sum;
                assert(host_ret == ETOYWASMRESTART);
                goto fail; /* not a failure */
after_return:;
                struct val r[1];
                exec_pop_vals(ctx, &ft->result, r);
                uint32_t v1 = r[0].u.i32;
                sum += v1;
        }
        result = sum;
        host_ret = 0;
fail:
        assert(IS_RESTARTABLE(host_ret) ||
               restart->restart_type == RESTART_NONE);
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32, result);
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static const struct host_func my_host_inst_funcs[] = {
        HOST_FUNC(my_host_inst_, load, "(i)i"),
        HOST_FUNC(my_host_inst_, load_call, "(i)i"),
        HOST_FUNC(my_host_inst_, load_call_add, "(i)i"),
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
