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
#include "endian.h"
#include "exec_context.h"
#include "host_instance.h"
#include "idalloc.h"
#include "instance.h"
#include "lock.h"
#include "mem.h"
#include "module.h"
#include "suspend.h"
#include "type.h"
#include "usched.h"
#include "wasi.h" /* wasi_convert_errno */
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

        struct mem_context *mctx;
};

/*
 * wasi_threads_instance_join: wait for completion of all threads
 * spawned by wasi:thread_spawn in the wasi-threads instance.
 */
static void wasi_threads_instance_join(struct wasi_threads_instance *wasi);

static struct cluster *
wasi_threads_cluster(struct wasi_threads_instance *inst);

static void wasi_threads_propagate_trap(struct wasi_threads_instance *wasi,
                                        const struct trap_info *trap);
static const struct trap_info *
wasi_threads_instance_get_trap(struct wasi_threads_instance *wasi);

#if defined(TOYWASM_USE_USER_SCHED)
static struct sched *
wasi_threads_sched(struct wasi_threads_instance *wasi)
{
        return &wasi->sched;
}
#endif

int
wasi_threads_instance_create(struct mem_context *mctx,
                             struct wasi_threads_instance **instp)
        NO_THREAD_SAFETY_ANALYSIS
{
        struct wasi_threads_instance *inst;

        inst = mem_zalloc(mctx, sizeof(*inst));
        if (inst == NULL) {
                return ENOMEM;
        }
        inst->mctx = mctx;
        /*
         * Note: wasi:thread_spawn uses negative values to indicate
         * an error.
         * Note: 0 is reserved.
         * Note: the upper-most 3 bits are reserved.
         */
        idalloc_init(&inst->tids, 1, 0x1fffffff);
        cluster_init(&inst->cluster);
#if defined(TOYWASM_USE_USER_SCHED)
        sched_init(&inst->sched);
#endif
        *instp = inst;
        return 0;
}

void
wasi_threads_instance_destroy(struct wasi_threads_instance *inst)
{
        struct mem_context *mctx = inst->mctx;
        idalloc_destroy(&inst->tids, mctx);
        cluster_destroy(&inst->cluster);
#if defined(TOYWASM_USE_USER_SCHED)
        sched_clear(&inst->sched);
#endif
        mem_free(mctx, inst, sizeof(*inst));
}

void
wasi_threads_setup_exec_context(struct wasi_threads_instance *wasi_threads,
                                struct exec_context *ctx)
{
        if (wasi_threads == NULL) {
                return;
        }
        struct cluster *c = wasi_threads_cluster(wasi_threads);
        ctx->cluster = c;
        toywasm_mutex_lock(&c->lock);
        cluster_add_thread(c); /* add ourselves */
        toywasm_mutex_unlock(&c->lock);
        c->interrupt = 0;
#if defined(TOYWASM_USE_USER_SCHED)
        ctx->sched = wasi_threads_sched(wasi_threads);
#endif
        /*
         * if none of threads explicitly exits or traps,
         * treat as if exit(0).
         */
        wasi_threads->trap.trapid = TRAP_VOLUNTARY_EXIT;
}

/*
 * 1. wait for the completion of the other threads
 *
 * 2. if necessary, replace the trap in *trapp with another one, which
 *    represents the exit status of the whole "process".
 *    in that case, the new trap might be the one owned by wasi_threads.
 */
void
wasi_threads_complete_exec(struct wasi_threads_instance *wasi_threads,
                           const struct trap_info **trapp)
{
        if (wasi_threads == NULL) {
                return;
        }
        const struct trap_info *trap = *trapp;
        if (trap != NULL) {
                wasi_threads_propagate_trap(wasi_threads, trap);
                wasi_threads_instance_join(wasi_threads);
                *trapp = wasi_threads_instance_get_trap(wasi_threads);
                xlog_trace("propagated trap: %u", (*trapp)->trapid);
        } else {
                wasi_threads_instance_join(wasi_threads);
        }
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

static void
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
        if (cluster_set_interrupt(&wasi->cluster)) {
                xlog_trace("Emulating proc_exit(0) on a return from _start");
        }
#endif
        cluster_remove_thread(&wasi->cluster); /* remove ourselves */
        toywasm_mutex_unlock(&wasi->cluster.lock);
#if defined(TOYWASM_USE_USER_SCHED)
        sched_run(&wasi->sched, NULL);
#endif
        cluster_join(&wasi->cluster);
}

static struct cluster *
wasi_threads_cluster(struct wasi_threads_instance *inst)
{
        return &inst->cluster;
}

static const struct trap_info *
wasi_threads_instance_get_trap(struct wasi_threads_instance *wasi)
{
        return &wasi->trap;
}

static bool
trap_is_local(const struct trap_info *trap)
{
        return trap->trapid == TRAP_VOLUNTARY_THREAD_EXIT;
}

static void
wasi_threads_propagate_trap(struct wasi_threads_instance *wasi,
                            const struct trap_info *trap)
{
        if (trap_is_local(trap)) {
                return;
        }
        toywasm_mutex_lock(&wasi->cluster.lock);
        /*
         * tell all threads to terminate.
         * propagate only the first trap.
         */
        if (cluster_set_interrupt(&wasi->cluster)) {
                xlog_trace("propagating a trap %u", trap->trapid);
                wasi->trap = *trap;
        } else {
                xlog_trace("interrupt already active");
        }
        toywasm_mutex_unlock(&wasi->cluster.lock);
}

struct thread_arg {
        struct wasi_threads_instance *wasi;
        struct mem_context *mctx;
        struct instance *inst;
        uint32_t user_arg;
        int32_t tid;
};

static int
exec_thread_start_func(struct exec_context *ctx, const struct thread_arg *arg)
{
        struct wasi_threads_instance *wasi = arg->wasi;

        struct val param[2];
        param[0].u.i32 = arg->tid;
        param[1].u.i32 = arg->user_arg;

        /* XXX should inherit exec_options from the parent? */
        ctx->cluster = wasi_threads_cluster(wasi);
#if defined(TOYWASM_USE_USER_SCHED)
        ctx->sched = wasi_threads_sched(wasi);
#endif

        /*
         * Note: the type of this function has already been confirmed by
         * wasi_threads_instance_set_thread_spawn_args.
         */
        const uint32_t funcidx = wasi->thread_start_funcidx;
        DEFINE_TYPES(static const, types, TYPE_i32, TYPE_i32);
        DEFINE_RESULTTYPE(static const, rt, &types, 2);
        int ret = exec_push_vals(ctx, &rt, param);
        if (ret != 0) {
                return ret;
        }
        return instance_execute_func_nocheck(ctx, funcidx);
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
                } else {
                        xlog_trace("%s: wasi_thread_start trapped %u: %s",
                                   __func__, ctx->trap.trapid,
                                   report_getmessage(ctx->report));
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
        idalloc_free(&wasi->tids, tid, wasi->mctx);
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
        exec_context_init(ctx, arg->inst, arg->mctx);
        ctx->exec_done = user_runner_exec_done;
        ctx->exec_done_arg = arg;
        ret = exec_thread_start_func(ctx, arg);
        if (IS_RESTARTABLE(ret)) {
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
        exec_context_init(ctx, arg->inst, arg->mctx);

        ret = exec_thread_start_func(ctx, arg);
        while (IS_RESTARTABLE(ret)) {
                suspend_parked(ctx->cluster);
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
                           (const void *)wasi->module,
                           (const void *)ctx->instance->module);
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
        /* REVISIT: it isn't appropraite to use wasi-threads mctx here */
        ret = instance_create(wasi->mctx, wasi->module, &inst, wasi->imports,
                              &report);
        if (ret != 0) {
                xlog_trace("%s: instance_create failed with %d: %s", __func__,
                           ret, report_getmessage(&report));
                report_clear(&report);
                goto fail;
        }
        report_clear(&report);
        toywasm_mutex_lock(&wasi->cluster.lock);
        ret = idalloc_alloc(&wasi->tids, &tid, wasi->mctx);
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
        arg->mctx = ctx->mctx; /* inherit mctx from the parent exec_context */

#if defined(TOYWASM_USE_USER_SCHED)
        ret = user_runner_exec_start(arg);
#else
        pthread_t t;
        ret = pthread_create(&t, NULL, runner, arg);
        if (ret != 0) {
                toywasm_mutex_lock(&wasi->cluster.lock);
                idalloc_free(&wasi->tids, tid, wasi->mctx);
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
        HOST_FUNC_TRACE;
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
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return 0;
}

static int
wasi_thread_spawn(struct exec_context *ctx, struct host_instance *hi,
                  const struct functype *ft, const struct cell *params,
                  struct cell *results)
{
        HOST_FUNC_TRACE;
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
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_func_copyout(ctx, &r, retp, sizeof(r), WASI_U32_ALIGN);
}

static int
wasi_thread_exit(struct exec_context *ctx, struct host_instance *hi,
                 const struct functype *ft, const struct cell *params,
                 struct cell *results)
{
        HOST_FUNC_TRACE;
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
        HOST_FUNC("thread-spawn", wasi_thread_spawn, "(ii)"),
        /*
         * https://github.com/WebAssembly/wasi-threads/pull/26
         * https://github.com/WebAssembly/wasi-libc/pull/387
         */
        HOST_FUNC("thread-spawn", wasi_thread_spawn_old, "(i)i"),
        /*
         * The "original" version.
         */
        HOST_FUNC("thread_spawn", wasi_thread_spawn_old, "(i)i"),

        /*
         * Note: thread_exit is not a part of the current wasi-threads.
         * It's implemented here just for my experiments.
         * cf. https://github.com/WebAssembly/wasi-threads/issues/7
         */
        HOST_FUNC("thread_exit", wasi_thread_exit, "()"),
};

const struct name wasi_threads_module_name = NAME_FROM_CSTR_LITERAL("wasi");

static const struct host_module module_wasi_threads = {
        .module_name = &wasi_threads_module_name,
        .funcs = wasi_threads_funcs,
        .nfuncs = ARRAYCOUNT(wasi_threads_funcs),
};

int
import_object_create_for_wasi_threads(
        struct mem_context *mctx, struct wasi_threads_instance *wasi_threads,
        struct import_object **impp)
{
        return import_object_create_for_host_funcs(mctx, &module_wasi_threads,
                                                   1, &wasi_threads->hi, impp);
}
