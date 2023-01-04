/*
 * wasi-threads proposal
 * https://github.com/WebAssembly/wasi-threads
 *
 * implemented separately from wasi_preview1 for now.
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "host_instance.h"
#include "idalloc.h"
#include "instance.h"
#include "lock.h"
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

        TOYWASM_MUTEX_DEFINE(lock);
        struct idalloc tids;
        pthread_cond_t cv;
        uint32_t nrunners;

        /* parameters for thread_spawn */
        struct module *module;
        const struct import_object *imports;
        uint32_t thread_start_funcidx;

        /*
         * for proc_exit and trap
         *
         * all threads poll this "interrupt" variable and terminate
         * themselves when necessary. definitely this is not the most
         * efficient design.
         * this design was chosen mainly because this implementation aims
         * to be portable to wasi-threads itself, which doesn't have any
         * async inter-thread communitation mechanisms like signals.
         */
        uint32_t interrupt;
        struct trap_info trap;
};

int
wasi_threads_instance_create(struct wasi_threads_instance **instp)
{
        struct wasi_threads_instance *inst;

        inst = zalloc(sizeof(*inst));
        if (inst == NULL) {
                return ENOMEM;
        }
        /*
         * Note: wasi:thread_spawn uses negative values to indicate
         * an error.
         * Note: 0 is reserved.
         * Note: the upper-most 3 bits are reserved.
         */
        idalloc_init(&inst->tids, 1, 0x1fffffff);
        toywasm_mutex_init(&inst->lock);
        int ret = pthread_cond_init(&inst->cv, NULL);
        assert(ret == 0);
        /*
         * if none of threads explicitly exits or traps,
         * treat as if exit(0).
         */
        inst->trap.trapid = TRAP_VOLUNTARY_EXIT;
        *instp = inst;
        return 0;
}

void
wasi_threads_instance_destroy(struct wasi_threads_instance *inst)
{
        idalloc_destroy(&inst->tids);
        toywasm_mutex_destroy(&inst->lock);
        int ret = pthread_cond_destroy(&inst->cv);
        assert(ret == 0);
        free(inst);
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
        /*
         * Note: this is normal when running non-threaded module.
         */
        xlog_trace("%s: ignoring error %d", __func__, ret);
        return 0;
}

void
wasi_threads_instance_join(struct wasi_threads_instance *wasi)
{
        toywasm_mutex_lock(&wasi->lock);
        while (wasi->nrunners > 0) {
                int ret = pthread_cond_wait(&wasi->cv, &wasi->lock.lock);
                assert(ret == 0);
        }
        toywasm_mutex_unlock(&wasi->lock);
}

const uint32_t *
wasi_threads_interrupt_pointer(struct wasi_threads_instance *inst)
{
        return &inst->interrupt;
}

const struct trap_info *
wasi_threads_instance_get_trap(struct wasi_threads_instance *wasi)
{
        return &wasi->trap;
}

static bool
trap_is_local(const struct trap_info *trap)
{
        return trap->trapid == TRAP_VOLUNTARY_THREAD_EXIT;
}

void
wasi_threads_propagate_trap(struct wasi_threads_instance *wasi,
                            const struct trap_info *trap)
{
        if (trap_is_local(trap)) {
                return;
        }
        toywasm_mutex_lock(&wasi->lock);
        /* propagate only the first one */
        if (!wasi->interrupt) {
                wasi->interrupt = 1; /* tell all threads to terminate */
                wasi->trap = *trap;
        }
        toywasm_mutex_unlock(&wasi->lock);
}

struct thread_arg {
        struct wasi_threads_instance *wasi;
        struct instance *inst;
        uint32_t user_arg;
        int32_t tid;
};

static void *
runner(void *vp)
{
        const struct thread_arg *arg = vp;
        struct wasi_threads_instance *wasi = arg->wasi;
        struct instance *inst = arg->inst;
        uint32_t tid;
        int ret;

        struct val param[2];
        param[0].u.i32 = tid = arg->tid;
        param[1].u.i32 = arg->user_arg;
        free(vp);

        struct exec_context ctx0;
        struct exec_context *ctx = &ctx0;
        exec_context_init(ctx, inst);
        ctx->intrp = wasi_threads_interrupt_pointer(wasi);

        /*
         * Note: the type of this function has already been confirmed by
         * wasi_threads_instance_set_thread_spawn_args.
         */
        ret = instance_execute_func_nocheck(ctx, wasi->thread_start_funcidx,
                                            param, NULL);
        if (ret == EFAULT && ctx->trapped) {
                wasi_threads_propagate_trap(wasi, &ctx->trap);
                if (ctx->trap.trapid == TRAP_VOLUNTARY_EXIT ||
                    ctx->trap.trapid == TRAP_VOLUNTARY_THREAD_EXIT) {
                        xlog_trace(
                                "%s: wasi_thread_start exited with %" PRIu32,
                                __func__, ctx->trap.exit_code);
                        ret = 0;
                } else if (ctx->report->msg != NULL) {
                        xlog_trace("%s: wasi_thread_start trapped %u: %s",
                                   __func__, ctx->trap.trapid,
                                   ctx->report->msg);
                } else {
                        xlog_trace("%s: wasi_thread_start trapped %u",
                                   __func__, ctx->trap.trapid);
                }
        }
        exec_context_clear(ctx);
        if (ret != 0) {
                /* XXX what to do for errors other than traps? */
                xlog_error("%s: instance_execute_func failed with %d",
                           __func__, ret);
                goto fail;
        }
fail:
        instance_destroy(inst);
        toywasm_mutex_lock(&wasi->lock);
        idalloc_free(&wasi->tids, tid);
        assert(wasi->nrunners > 0);
        wasi->nrunners--;
        if (wasi->nrunners == 0) {
                ret = pthread_cond_signal(&wasi->cv);
                assert(ret == 0);
        }
        toywasm_mutex_unlock(&wasi->lock);
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
        struct instance *inst = NULL;
        struct thread_arg *arg = NULL;
        uint32_t tid;
        int ret;

        /*
         * When multiple modules are involved, it isn't too obvious
         * which module to re-instantiate on thread_spawn.
         * For now, only allow the simplest case.
         * cf. https://github.com/WebAssembly/wasi-threads/issues/13
         */
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

        struct report report;
        report_init(&report);
        ret = instance_create(wasi->module, &inst, wasi->imports, &report);
        if (report.msg != NULL) {
                xlog_trace("%s: instance_create: %s", __func__, report.msg);
        }
        report_clear(&report);
        if (ret != 0) {
                xlog_trace("%s: instance_create failed with %d", __func__,
                           ret);
                goto fail;
        }
        toywasm_mutex_lock(&wasi->lock);
        ret = idalloc_alloc(&wasi->tids, &tid);
        toywasm_mutex_unlock(&wasi->lock);
        if (ret != 0) {
                xlog_trace("%s: TID allocation failed with %d", __func__, ret);
                goto fail;
        }

        arg->wasi = wasi;
        arg->inst = inst;
        arg->tid = tid;
        arg->user_arg = user_arg;

        pthread_t t;
        ret = pthread_create(&t, NULL, runner, arg);
        if (ret != 0) {
                toywasm_mutex_lock(&wasi->lock);
                idalloc_free(&wasi->tids, tid);
                toywasm_mutex_unlock(&wasi->lock);
                xlog_trace("%s: pthread_create failed with %d", __func__, ret);
                goto fail;
        }
        inst = NULL;
        arg = NULL;

        ret = pthread_detach(t);
        if (ret != 0) {
                /* log and ignore */
                xlog_error("pthread_detach failed with %d", ret);
                ret = 0;
        }

        toywasm_mutex_lock(&wasi->lock);
        assert(wasi->nrunners < UINT32_MAX);
        wasi->nrunners++;
        toywasm_mutex_unlock(&wasi->lock);

fail:
        if (inst != NULL) {
                instance_destroy(inst);
        }
        free(arg);
        int32_t result;
        if (ret != 0) {
                /* negative errno on error */
                result = -wasi_convert_errno(ret);
                xlog_trace("%s failed with %d", __func__, ret);
        } else {
                result = tid;
                xlog_trace("%s succeeded tid %u", __func__, tid);
        }
        HOST_FUNC_RESULT_SET(ft, results, 0, i32, result);
        return 0;
}

static int
wasi_thread_exit(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        WASI_TRACE;
        xlog_trace("wasi_thread_exit");
        return trap_with_id(ctx, TRAP_VOLUNTARY_THREAD_EXIT, "thread_exit");
}

const struct host_func wasi_threads_funcs[] = {
        WASI_HOST_FUNC(thread_spawn, "(i)i"),
        /*
         * Note: thread_exit is not a part of the current wasi-threads.
         * It's implemented here just for my experiments.
         * cf. https://github.com/WebAssembly/wasi-threads/issues/7
         */
        WASI_HOST_FUNC(thread_exit, "()"),
};

const struct name wasi_threads_module_name = NAME_FROM_CSTR_LITERAL("wasi");

static const struct host_module module_wasi_threads = {
        .module_name = &wasi_threads_module_name,
        .funcs = wasi_threads_funcs,
        .nfuncs = ARRAYCOUNT(wasi_threads_funcs),
};

int
import_object_create_for_wasi_threads(struct wasi_threads_instance *th,
                                      struct import_object **impp)
{
        return import_object_create_for_host_funcs(&module_wasi_threads, 1,
                                                   &th->hi, impp);
}
