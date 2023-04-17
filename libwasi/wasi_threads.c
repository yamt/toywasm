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

#include "cluster.h"
#include "context.h"
#include "endian.h"
#include "idalloc.h"
#include "instance.h"
#include "lock.h"
#include "module.h"
#include "type.h"
#include "usched.h"
#include "wasi_impl.h"
#include "wasi_threads.h"
#include "wasi_threads_abi.h"
#include "xlog.h"

#if !defined(TOYWASM_ENABLE_WASM_THREADS)
#error TOYWASM_ENABLE_WASI_THREADS requires TOYWASM_ENABLE_WASM_THREADS
#endif

struct wasi_threads_instance {
        struct host_instance hi;
        struct cluster cluster;

        struct idalloc tids;

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
        struct trap_info trap;

#if defined(TOYWASM_USE_USER_SCHED)
        struct sched sched;
#endif
};

#if defined(TOYWASM_USE_USER_SCHED)
struct sched *
wasi_threads_sched(struct wasi_threads_instance *wasi)
{
        return &wasi->sched;
}
#endif

int
wasi_threads_instance_create(struct wasi_threads_instance **instp)
        NO_THREAD_SAFETY_ANALYSIS
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
        cluster_init(&inst->cluster);
        cluster_add_thread(&inst->cluster); /* count the main thread */
        /*
         * if none of threads explicitly exits or traps,
         * treat as if exit(0).
         */
        inst->trap.trapid = TRAP_VOLUNTARY_EXIT;
#if defined(TOYWASM_USE_USER_SCHED)
        sched_init(&inst->sched);
#endif
        *instp = inst;
        return 0;
}

void
wasi_threads_instance_destroy(struct wasi_threads_instance *inst)
{
        idalloc_destroy(&inst->tids);
        cluster_destroy(&inst->cluster);
#if defined(TOYWASM_USE_USER_SCHED)
        sched_clear(&inst->sched);
#endif
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
        toywasm_mutex_lock(&wasi->cluster.lock);
#if 1
        /*
         * https://github.com/WebAssembly/wasi-threads/issues/21
         *
         * option b.
         * proc_exit(0) equivalent. terminate all other threads.
         */
        if (!wasi->cluster.interrupt) {
                xlog_trace("Emulating proc_exit(0) on a return from _start");
                wasi->cluster.interrupt = 1;
        }
#endif
        cluster_remove_thread(&wasi->cluster); /* remove ourselves */
        toywasm_mutex_unlock(&wasi->cluster.lock);
#if defined(TOYWASM_USE_USER_SCHED)
        sched_run(&wasi->sched, NULL);
#endif
        cluster_join(&wasi->cluster);
}

const atomic_uint *
wasi_threads_interrupt_pointer(struct wasi_threads_instance *inst)
{
        return &inst->cluster.interrupt;
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
        toywasm_mutex_lock(&wasi->cluster.lock);
        /* propagate only the first one */
        if (!wasi->cluster.interrupt) {
                /* tell all threads to terminate */
                wasi->cluster.interrupt = 1;
                wasi->trap = *trap;
        }
        toywasm_mutex_unlock(&wasi->cluster.lock);
}

struct thread_arg {
        struct wasi_threads_instance *wasi;
        struct instance *inst;
        uint32_t user_arg;
        int32_t tid;
};

static int
exec_thread_start_func(struct exec_context *ctx, const struct thread_arg *arg)
{
        struct wasi_threads_instance *wasi = arg->wasi;
        uint32_t tid;

        struct val param[2];
        param[0].u.i32 = tid = arg->tid;
        param[1].u.i32 = arg->user_arg;

        /* XXX should inherit exec_options from the parent? */
        ctx->intrp = wasi_threads_interrupt_pointer(wasi);
#if defined(TOYWASM_USE_USER_SCHED)
        ctx->sched = wasi_threads_sched(wasi);
#endif

        /*
         * Note: the type of this function has already been confirmed by
         * wasi_threads_instance_set_thread_spawn_args.
         */
        return instance_execute_func_nocheck(ctx, wasi->thread_start_funcidx,
                                             param, NULL);
}

static void
done_thread_start_func(struct exec_context *ctx, const struct thread_arg *arg,
                       int ret)
{
        struct wasi_threads_instance *wasi = arg->wasi;
        struct instance *inst = arg->inst;
        uint32_t tid = arg->tid;

        if (ret == ETOYWASMTRAP) {
                assert(ctx->trapped);
                wasi_threads_propagate_trap(wasi, &ctx->trap);
                if (ctx->trap.trapid == TRAP_VOLUNTARY_EXIT ||
                    ctx->trap.trapid == TRAP_VOLUNTARY_THREAD_EXIT) {
                        xlog_trace(
                                "%s: wasi_thread_start exited voluntarily %u",
                                __func__, ctx->trap.trapid);
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
        toywasm_mutex_lock(&wasi->cluster.lock);
        idalloc_free(&wasi->tids, tid);
        cluster_remove_thread(&wasi->cluster);
        toywasm_mutex_unlock(&wasi->cluster.lock);
}

#if defined(TOYWASM_USE_USER_SCHED)
static void
user_runner_exec_done(struct exec_context *ctx)
{
        void *vp = ctx->exec_done_arg;
        int ret = ctx->exec_ret;

        const struct thread_arg *arg = vp;
        done_thread_start_func(ctx, arg, ret);
        free(vp);
        free(ctx);
}

static int
user_runner_exec_start(struct thread_arg *arg)
{
        struct exec_context *ctx = malloc(sizeof(*ctx));
        int ret;
        if (ctx == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        exec_context_init(ctx, arg->inst);
        ctx->exec_done = user_runner_exec_done;
        ctx->exec_done_arg = arg;
        ret = exec_thread_start_func(ctx, arg);
        if (ret == ETOYWASMRESTART) {
                struct sched *sched = wasi_threads_sched(arg->wasi);
                sched_enqueue(sched, ctx);
        } else {
                ctx->exec_done(ctx);
        }
        ret = 0;
fail:
        return ret;
}
#else  /* defined(TOYWASM_USE_USER_SCHED) */
static void *
runner(void *vp)
{
        const struct thread_arg *arg = vp;
        int ret;

        struct exec_context ctx0;
        struct exec_context *ctx = &ctx0;
        exec_context_init(ctx, arg->inst);

        ret = exec_thread_start_func(ctx, arg);
        while (ret == ETOYWASMRESTART) {
                xlog_trace("%s: restarting execution\n", __func__);
                ret = instance_execute_continue(ctx);
        }
        done_thread_start_func(ctx, arg, ret);
        free(vp);
        return NULL;
}
#endif /* defined(TOYWASM_USE_USER_SCHED) */

static int
wasi_thread_spawn_common(struct exec_context *ctx,
                         struct wasi_threads_instance *wasi, uint32_t user_arg,
                         uint32_t *tidp)
{
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
        toywasm_mutex_lock(&wasi->cluster.lock);
        ret = idalloc_alloc(&wasi->tids, &tid);
        if (ret != 0) {
                toywasm_mutex_unlock(&wasi->cluster.lock);
                xlog_trace("%s: TID allocation failed with %d", __func__, ret);
                goto fail;
        }
        cluster_add_thread(&wasi->cluster);
        toywasm_mutex_unlock(&wasi->cluster.lock);

        arg->wasi = wasi;
        arg->inst = inst;
        arg->tid = tid;
        arg->user_arg = user_arg;

#if defined(TOYWASM_USE_USER_SCHED)
        ret = user_runner_exec_start(arg);
#else
        pthread_t t;
        ret = pthread_create(&t, NULL, runner, arg);
        if (ret != 0) {
                toywasm_mutex_lock(&wasi->cluster.lock);
                idalloc_free(&wasi->tids, tid);
                cluster_remove_thread(&wasi->cluster);
                toywasm_mutex_unlock(&wasi->cluster.lock);
                xlog_trace("%s: pthread_create failed with %d", __func__, ret);
                goto fail;
        }

        ret = pthread_detach(t);
        if (ret != 0) {
                /* log and ignore */
                xlog_error("pthread_detach failed with %d", ret);
                ret = 0;
        }
#endif
        inst = NULL;
        arg = NULL;

fail:
        if (inst != NULL) {
                instance_destroy(inst);
        }
        free(arg);
        if (ret == 0) {
                *tidp = tid;
        }
        return ret;
}

static int
wasi_thread_spawn_old(struct exec_context *ctx, struct host_instance *hi,
                      const struct functype *ft, const struct cell *params,
                      struct cell *results)
{
        WASI_TRACE;
        struct wasi_threads_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t user_arg = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t tid;

        int ret = wasi_thread_spawn_common(ctx, wasi, user_arg, &tid);
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
wasi_thread_spawn(struct exec_context *ctx, struct host_instance *hi,
                  const struct functype *ft, const struct cell *params,
                  struct cell *results)
{
        WASI_TRACE;
        struct wasi_threads_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t user_arg = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t tid;

        int ret = wasi_thread_spawn_common(ctx, wasi, user_arg, &tid);
        struct wasi_thread_spawn_result r;
        memset(&r, 0, sizeof(r));
        if (ret != 0) {
                xlog_trace("%s failed with %d", __func__, ret);
                r.is_error = 1;
                /* EAGAIN is the only defined error for now. */
                r.u.error = WASI_THREADS_ERROR_AGAIN;
        } else {
                xlog_trace("%s succeeded tid %u", __func__, tid);
                r.is_error = 0;
                le32_encode(&r.u.tid, tid);
        }
        return wasi_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
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
        /*
         * Note: We keep a few old ABIs of thread-spawn for a while to
         * ease experiment/migration/testing.
         */

        /*
         * https://github.com/WebAssembly/wasi-threads/pull/28
         * https://github.com/WebAssembly/wasi-libc/pull/385
         */
        WASI_HOST_FUNC2("thread-spawn", wasi_thread_spawn, "(ii)"),
        /*
         * https://github.com/WebAssembly/wasi-threads/pull/26
         * https://github.com/WebAssembly/wasi-libc/pull/387
         */
        WASI_HOST_FUNC2("thread-spawn", wasi_thread_spawn_old, "(i)i"),
        /*
         * The "original" version.
         */
        WASI_HOST_FUNC2("thread_spawn", wasi_thread_spawn_old, "(i)i"),

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
