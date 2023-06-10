#include <toywasm/exec_context.h>
#include <toywasm/fileio.h>
#include <toywasm/instance.h>
#include <toywasm/load_context.h>
#include <toywasm/module.h>
#include <toywasm/report.h>
#include <toywasm/type.h>
#include <toywasm/wasi.h>
#include <toywasm/xlog.h>

#include "runwasi.h"

int
runwasi(const char *filename, unsigned int ndirs, char **dirs,
        unsigned int nenvs, char **envs, int argc, char **argv,
        uint32_t *wasi_exit_code_p)
{
        struct module *m = NULL;
        struct wasi_instance *wasi = NULL;
        struct import_object *wasi_import_object = NULL;
        int ret;

        /*
         * load a module
         */
        uint8_t *p;
        size_t sz;
        ret = map_file(filename, (void **)&p, &sz);
        if (ret != 0) {
                xlog_error("map_file failed with %d", ret);
                goto fail;
        }
        struct load_context lctx;
        load_context_init(&lctx);
        ret = module_create(&m, p, p + sz, &lctx);
        if (ret != 0) {
                xlog_error("module_load failed with %d", ret);
                goto fail;
        }
        load_context_clear(&lctx);

        /*
         * find the entry point
         */
        uint32_t funcidx;
        struct name name = NAME_FROM_CSTR_LITERAL("_start");
        ret = module_find_export_func(m, &name, &funcidx);
        if (ret != 0) {
                xlog_error("module_find_export_func failed with %d", ret);
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
         * create a wasi instance
         */
        ret = wasi_instance_create(&wasi);
        if (ret != 0) {
                xlog_error("wasi_instance_create failed with %d", ret);
                goto fail;
        }
        wasi_instance_set_args(wasi, argc, argv);
        wasi_instance_set_environ(wasi, nenvs, envs);
        unsigned int i;
        for (i = 0; i < ndirs; i++) {
                ret = wasi_instance_prestat_add(wasi, dirs[i]);
                if (ret != 0) {
                        xlog_error("wasi_instance_prestat_add failed with %d",
                                   ret);
                        goto fail;
                }
        }
        ret = import_object_create_for_wasi(wasi, &wasi_import_object);
        if (ret != 0) {
                xlog_error("import_object_create_for_wasi failed with %d",
                           ret);
                goto fail;
        }

        /*
         * instantiate the module
         */
        struct instance *inst;
        struct report report;
        report_init(&report);
        ret = instance_create(m, &inst, wasi_import_object, &report);
        if (ret != 0) {
                xlog_error("instance_create failed with %d", ret);
                report_clear(&report);
                goto fail;
        }
        report_clear(&report);

        /*
         * execute the module
         */
        struct exec_context ectx;
        exec_context_init(&ectx, inst);
        ret = instance_execute_func(&ectx, funcidx, pt, rt, NULL, NULL);
        ret = instance_execute_handle_restart(&ectx, ret);
        uint32_t wasi_exit_code = 0;
        if (ret == ETOYWASMTRAP) {
                const struct trap_info *trap = &ectx.trap;
                if (trap->trapid == TRAP_VOLUNTARY_EXIT) {
                        wasi_exit_code = wasi_instance_exit_code(wasi);
                } else {
                        xlog_error("got a trap %u",
                                   (unsigned int)trap->trapid);
                        exec_context_clear(&ectx);
                        goto fail;
                }
        } else if (ret != 0) {
                xlog_error("instance_execute_func failed with %d", ret);
                exec_context_clear(&ectx);
                goto fail;
        }
        exec_context_clear(&ectx);

        /*
         * clean up
         */
fail:
        if (inst != NULL) {
                instance_destroy(inst);
        }
        if (wasi_import_object != NULL) {
                import_object_destroy(wasi_import_object);
        }
        if (wasi != NULL) {
                wasi_instance_destroy(wasi);
        }
        if (m != NULL) {
                module_destroy(m);
        }
        if (p != NULL) {
                unmap_file(p, sz);
        }
        return ret;
}
