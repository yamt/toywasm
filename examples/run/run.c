#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <toywasm/exec_context.h>
#include <toywasm/fileio.h>
#include <toywasm/instance.h>
#include <toywasm/load_context.h>
#include <toywasm/mem.h>
#include <toywasm/module.h>
#include <toywasm/report.h>
#include <toywasm/toywasm_config.h>
#include <toywasm/type.h>
#include <toywasm/xlog.h>

int
main(int argc, char **argv)
{
        uint8_t *p = NULL;
        size_t sz = 0; /* MSVC C4701 */
        struct module *m = NULL;
        struct instance *inst = NULL;
        int ret;

#if defined(TOYWASM_ENABLE_TRACING)
        xlog_tracing = 1;
#endif

        struct mem_context mctx0;
        struct mem_context *mctx = &mctx0;
        mem_context_init(mctx);

        if (argc != 3) {
                xlog_error("arg");
                ret = EINVAL;
                goto fail;
        }
        const char *filename = argv[1];
        const char *func_name = argv[2];

        /*
         * load a module
         */
        fprintf(stderr, "reading a file %s...\n", filename);
        ret = map_file(filename, (void **)&p, &sz);
        if (ret != 0) {
                xlog_error("map_file failed with %d", ret);
                goto fail;
        }
        fprintf(stderr, "reading the module...\n");
        struct load_context lctx;
        load_context_init(&lctx, mctx);
        ret = module_create(&m, p, p + sz, &lctx);
        if (ret != 0) {
                xlog_error("module_load failed with %d: %s", ret,
                           report_getmessage(&lctx.report));
                load_context_clear(&lctx);
                goto fail;
        }
        load_context_clear(&lctx);

        /*
         * find the entry point
         */
        fprintf(stderr, "looking for the exported function %s...\n",
                func_name);
        uint32_t funcidx;
        struct name name;
        set_name_cstr(&name, func_name);
        ret = module_find_export(m, &name, EXTERNTYPE_FUNC, &funcidx);
        if (ret != 0) {
                xlog_error("module_find_export failed with %d", ret);
                goto fail;
        }
        const struct functype *ft = module_functype(m, funcidx);
        const struct resulttype *pt = &ft->parameter;
        const struct resulttype *rt = &ft->result;
        if (pt->ntypes != 0 || rt->ntypes != 0) {
                xlog_error("unexpected type of _start");
                goto fail;
        }

        /*
         * instantiate the module
         */
        fprintf(stderr, "instantiating the modules...\n");
        struct report report;
        report_init(&report);
        ret = instance_create(mctx, m, &inst, NULL, &report);
        if (ret != 0) {
                const char *msg = report_getmessage(&report);
                xlog_error("instance_create failed with %d: %s", ret, msg);
                report_clear(&report);
                goto fail;
        }
        report_clear(&report);

        /*
         * execute the module
         */
        fprintf(stderr, "executing the function...\n");
        struct exec_context ectx;
        exec_context_init(&ectx, inst, mctx);
        ret = instance_execute_func(&ectx, funcidx, pt, rt);
        ret = instance_execute_handle_restart(&ectx, ret);
        if (ret == ETOYWASMTRAP) {
                const struct trap_info *trap = &ectx.trap;
                xlog_error("got a trap %u: %s", (unsigned int)trap->trapid,
                           report_getmessage(ectx.report));
                exec_context_clear(&ectx);
                goto fail;
        } else if (ret != 0) {
                xlog_error("instance_execute_func failed with %d", ret);
                exec_context_clear(&ectx);
                goto fail;
        }
        exec_context_clear(&ectx);
        fprintf(stderr, "successfully executed the function!\n");
        ret = 0;
fail:
        /*
         * clean up
         */
        fprintf(stderr, "cleaning up...\n");
        if (inst != NULL) {
                instance_destroy(inst);
        }
        if (m != NULL) {
                module_destroy(mctx, m);
        }
        if (p != NULL) {
                unmap_file(p, sz);
        }
        mem_context_clear(mctx);
        if (ret != 0) {
                exit(1);
        }
        exit(0);
}
