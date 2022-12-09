/*
 * wasi-threads proposal
 * https://github.com/WebAssembly/wasi-threads
 *
 * implemented separately from wasi_preview1 for now.
 */

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "host_instance.h"
#include "instance.h"
#include "module.h"
#include "type.h"
#include "wasi_impl.h"
#include "wasi_threads.h"
#include "xlog.h"

#if !defined(TOYWASM_ENABLE_WASM_THREADS)
#error TOYWASM_ENABLE_WASI_THREADS requires TOYWASM_ENABLE_WASM_THREADS
#endif

struct wasi_threads_instance {
        struct host_instance hi;

        /* parameters for thread_spawn */
        struct module *module;
        const struct import_object *imports;
        uint32_t thread_start_funcidx;
};

int
wasi_threads_instance_create(struct wasi_threads_instance **instp)
{
        struct wasi_threads_instance *inst;

        inst = zalloc(sizeof(*inst));
        if (inst == NULL) {
                return ENOMEM;
        }
        *instp = inst;
        return 0;
}

void
wasi_threads_instance_destroy(struct wasi_threads_instance *inst)
{
        free(inst);
}

static int
check_functype_with_string(struct module *m, uint32_t funcidx, const char *sig)
{
        const struct functype *ft = module_functype(m, funcidx);
        struct functype *sig_ft;
        int ret;

        ret = functype_from_string(sig, &sig_ft);
        if (ret != 0) {
                return ret;
        }
        ret = 0;
        if (compare_functype(ft, sig_ft)) {
                ret = EINVAL;
        }
        functype_free(sig_ft);
        return ret;
}

int
wasi_threads_instance_set_thread_spawn_args(
        struct wasi_threads_instance *inst, struct module *m,
        const struct import_object *imports)
{
        const char *funcname = "wasi_thread_start";
        uint32_t funcidx;
        int ret;

        struct name funcname_name;
        funcname_name.data = funcname;
        funcname_name.nbytes = strlen(funcname);
        ret = module_find_export_func(m, &funcname_name, &funcidx);
        if (ret != 0) {
                xlog_trace("%s: start func not found %d", __func__, ret);
                goto fail;
        }
        ret = check_functype_with_string(m, funcidx, "(ii)");
        if (ret != 0) {
                xlog_trace("%s: func type mismatch", __func__);
                goto fail;
        }
        inst->module = m;
        inst->imports = imports;
        inst->thread_start_funcidx = funcidx;
        return 0;
fail:
        return ret;
}

struct thread_arg {
        struct wasi_threads_instance *wasi;
        struct instance *inst;
        uint32_t user_arg;
        int32_t tid;
};

static int
instance_execute_func_nocheck(struct exec_context *ctx, uint32_t funcidx,
                              const struct val *params, struct val *results)
{
        struct module *m = ctx->instance->module;
        const struct functype *ft = module_functype(m, funcidx);
        const struct resulttype *ptype = &ft->parameter;
        const struct resulttype *rtype = &ft->result;
        return instance_execute_func(ctx, funcidx, ptype, rtype, params,
                                     results);
}

static void *
runner(void *vp)
{
        const struct thread_arg *arg = vp;
        const struct wasi_threads_instance *wasi = arg->wasi;
        struct instance *inst = arg->inst;
        int ret;

        struct val param[2];
        param[0].u.i32 = arg->tid;
        param[1].u.i32 = arg->user_arg;

        struct exec_context ctx0;
        struct exec_context *ctx = &ctx0;
        exec_context_init(ctx, inst);
        ret = instance_execute_func_nocheck(ctx, wasi->thread_start_funcidx,
                                            param, NULL);
        if (ret == EFAULT && ctx->trapped) {
                if (ctx->trapid == TRAP_VOLUNTARY_EXIT) {
                        xlog_trace(
                                "%s: wasi_thread_start exited with %" PRIu32,
                                __func__, ctx->exit_code);
                        ret = ctx->exit_code;
                } else if (ctx->report->msg != NULL) {
                        xlog_trace("%s: wasi_thread_start trapped %u: %s",
                                   __func__, ctx->trapid, ctx->report->msg);
                } else {
                        xlog_trace("%s: wasi_thread_start trapped %u",
                                   __func__, ctx->trapid);
                }
        }
        exec_context_clear(ctx);
        if (ret != 0) {
                /* what to do? */
                xlog_error("%s: instance_execute_func failed with %d",
                           __func__, ret);
                goto fail;
        }
fail:
        instance_destroy(inst);
        free(vp);
        return NULL;
}

static int
wasi_thread_spawn(struct exec_context *ctx, struct host_instance *hi,
                  const struct functype *ft, const struct cell *params,
                  struct cell *results)
{
        WASI_TRACE;
        struct wasi_threads_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t user_arg = HOST_FUNC_PARAM(ft, params, 0, i32);
        struct thread_arg *arg = NULL;
        int32_t tid;
        int ret;

        if (wasi->module != ctx->instance->module) {
                xlog_trace("%s: module mismatch: %p != %p", __func__,
                           wasi->module, ctx->instance->module);
                ret = EPROTO;
                goto fail;
        }

        arg = malloc(sizeof(*arg));
        if (arg == NULL) {
                xlog_trace("%s: malloc failed", __func__);
                ret = ENOMEM;
                goto fail;
        }

        struct instance *inst;
        struct report report;
        report_init(&report);
        ret = instance_create(wasi->module, &inst, ctx->instance,
                              wasi->imports, &report);
        if (report.msg != NULL) {
                xlog_trace("%s: instance_create: %s", __func__, report.msg);
        }
        report_clear(&report);
        if (ret != 0) {
                xlog_trace("%s: instance_create failed with %d", __func__,
                           ret);
                goto fail;
        }
        tid = 1; /* XXX */

        arg->wasi = wasi;
        arg->inst = inst;
        arg->tid = tid;
        arg->user_arg = user_arg;

        pthread_t t;
        ret = pthread_create(&t, NULL, runner, arg);
        if (ret != 0) {
                xlog_trace("%s: pthread_create failed with %d", __func__, ret);
                goto fail;
        }
        arg = NULL;

        /*
         * XXX consider a nicer api which can actually manage threads.
         * for now, just detach and forget.
         */
        ret = pthread_detach(t);
        if (ret != 0) {
                /* log and ignore */
                xlog_error("pthread_detach failed with %d", ret);
                ret = 0;
        }
fail:
        free(arg);
        int32_t result;
        if (ret != 0) {
                /* negative errno on error */
                result = -wasi_convert_errno(ret);
        } else {
                result = tid;
        }
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, result);
        return 0;
}

const struct host_func wasi_threads_funcs[] = {
        WASI_HOST_FUNC(thread_spawn, "(i)i"),
};

struct name wasi_threads_module_name = NAME_FROM_CSTR_LITERAL("wasi");

int
import_object_create_for_wasi_threads(struct wasi_threads_instance *th,
                                      struct import_object **impp)
{
        return import_object_create_for_host_funcs(
                &wasi_threads_module_name, wasi_threads_funcs,
                ARRAYCOUNT(wasi_threads_funcs), &th->hi, impp);
}
