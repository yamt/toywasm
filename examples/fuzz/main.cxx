#include <stddef.h>

#include <toywasm/exec_context.h>
#include <toywasm/instance.h>
#include <toywasm/load_context.h>
#include <toywasm/mem.h>
#include <toywasm/module.h>
#include <toywasm/report.h>

extern "C" int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
        struct mem_context mctx;
        mem_context_init(&mctx);
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
        struct instance *inst;
        struct report report;
        report_init(&report);
        ret = instance_create_no_init(&mctx, m, &inst, NULL, &report);
        report_clear(&report);
        if (ret != 0) {
                goto fail_instantiate;
        }
        struct exec_context ectx;
        exec_context_init(&ectx, inst, &mctx);
        /* avoid infinite loops in the start function */
        const static atomic_uint one = 1;
        ectx.intrp = &one;
        ectx.user_intr_delay = 25;
        ret = instance_execute_init(&ectx);
        exec_context_clear(&ectx);
        instance_destroy(inst);
fail_instantiate:
        module_destroy(&mctx, m);
fail_load:
        mem_context_clear(&mctx);
        return 0;
}
