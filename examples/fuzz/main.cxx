#include "fuzz_config.h"

#include <stddef.h>
#if defined(FUZZ_WASI)
#include <unistd.h>
#endif

#include <toywasm/exec_context.h>
#include <toywasm/instance.h>
#include <toywasm/load_context.h>
#include <toywasm/mem.h>
#include <toywasm/module.h>
#include <toywasm/report.h>
#include <toywasm/type.h>
#if defined(FUZZ_WASI)
#include <toywasm/wasi.h>
#endif

static void
setup_exec_context(struct exec_context *ectx)
{
        /*
         * avoid infinite loops
         *
         * note: this raises ETOYWASMUSERINTERRUPT
         * after executing about
         * (user_intr_delay + 1) * CHECK_INTERVAL_DEFAULT
         * call/branch instructions.
         */
        const static atomic_uint one = 1;
        ectx->intrp = &one;
        ectx->user_intr_delay = 0;

        /* use moderate limits to avoid long execution */
        ectx->options.max_frames = 100;
        ectx->options.max_stackcells = 1000;
}

extern "C" int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
        struct import_object *wasi_import_object = NULL;
#if defined(FUZZ_WASI)
        static const char *args[] = {
                "dummy",
                "args",
        };
#endif
        struct mem_context mctx;
        mem_context_init(&mctx);
        /*
         * limit the heap usage to a moderate amount.
         * otherwise, it can easily reach libFuzzer's RSS/malloc limit.
         */
        mctx.limit = 1 * 1024 * 1024;
        struct module *m;
        int ret;
        struct load_context ctx;
        load_context_init(&ctx, &mctx);
        ret = module_create(&m, data, data + size, &ctx);
        load_context_clear(&ctx);
        if (ret != 0) {
                goto fail_load;
        }
#if defined(FUZZ_WASI)
        struct wasi_instance *wasi;
        ret = wasi_instance_create(&mctx, &wasi);
        if (ret != 0) {
                goto fail_wasi;
        }
        wasi_instance_set_args(wasi, 2, args);
        wasi_instance_set_environ(wasi, 2, args);
        ret = import_object_create_for_wasi(&mctx, wasi, &wasi_import_object);
        if (ret != 0) {
                goto fail_wasi_impobj;
        }
        int pipes[2];
        ret = pipe(pipes);
        if (ret != 0) {
                goto fail_pipe;
        }
        ret = wasi_instance_add_hostfd(wasi, 0, pipes[0]);
        if (ret != 0) {
                goto fail_wasi_stdio;
        }
        ret = wasi_instance_add_hostfd(wasi, 1, pipes[1]);
        if (ret != 0) {
                goto fail_wasi_stdio;
        }
        ret = wasi_instance_add_hostfd(wasi, 2, pipes[1]);
        if (ret != 0) {
                goto fail_wasi_stdio;
        }
#endif
        struct instance *inst;
        struct report report;
        report_init(&report);
        ret = instance_create_no_init(&mctx, m, &inst, wasi_import_object,
                                      &report);
        report_clear(&report);
        if (ret != 0) {
                goto fail_instantiate;
        }
        struct exec_context ectx;
        exec_context_init(&ectx, inst, &mctx);
        setup_exec_context(&ectx);
        ret = instance_execute_init(&ectx);
        if (ret == ETOYWASMRESTART) {
                ret = instance_execute_handle_restart_once(&ectx, ret);
        }
        exec_context_clear(&ectx);
        if (ret != 0) {
                goto fail_exec_init;
        }
        /* try to execute all exported functions */
        uint32_t i;
        for (i = 0; i < m->nexports; i++) {
                const struct wasm_export *ex = &m->exports[i];
                if (ex->desc.type != EXTERNTYPE_FUNC) {
                        continue;
                }
                uint32_t funcidx = ex->desc.idx;
                const struct functype *ft = module_functype(m, funcidx);
                const struct resulttype *param = &ft->parameter;
                const struct resulttype *result = &ft->result;
                uint32_t nvals = param->ntypes;
                struct val *vals = NULL;
                if (nvals > 0) {
                        vals = (struct val *)mem_calloc(&mctx, sizeof(*vals),
                                                        nvals);
                        if (vals == NULL) {
                                goto fail_exec_func;
                        }
                }
                exec_context_init(&ectx, inst, &mctx);
                setup_exec_context(&ectx);
                ret = exec_push_vals(&ectx, param, vals);
                if (vals != NULL) {
                        mem_free(&mctx, vals, sizeof(*vals) * nvals);
                }
                if (ret != 0) {
                        goto fail_exec_func;
                }
                ret = instance_execute_func(&ectx, funcidx, param, result);
                if (ret == ETOYWASMRESTART) {
                        ret = instance_execute_handle_restart_once(&ectx, ret);
                }
fail_exec_func:
                exec_context_clear(&ectx);
        }
fail_exec_init:
        instance_destroy(inst);
fail_instantiate:
#if defined(FUZZ_WASI)
        import_object_destroy(&mctx, wasi_import_object);
fail_wasi_impobj:
fail_wasi_stdio:
        close(pipes[0]);
        close(pipes[1]);
fail_pipe:
        wasi_instance_destroy(wasi);
fail_wasi:
#endif
        module_destroy(&mctx, m);
fail_load:
        mem_context_clear(&mctx);
        return 0;
}
