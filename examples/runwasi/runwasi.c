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
runwasi_module(struct mem_context *mctx, const struct module *m,
               unsigned int ndirs, char **dirs, unsigned int nenvs,
               const char *const *envs, int argc, const char *const *argv,
               const int stdio_fds[3], struct import_object *base_imports,
               uint32_t *wasi_exit_code_p)
{
        struct wasi_instance *wasi = NULL;
        struct import_object *wasi_import_object = NULL;
        struct instance *inst = NULL;
        int ret;

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
        ret = wasi_instance_create(mctx, &wasi);
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
        for (i = 0; i < 3; i++) {
                int hostfd = stdio_fds[i];
                if (hostfd == -1) {
                        continue;
                }
                ret = wasi_instance_add_hostfd(wasi, i, hostfd);
                if (ret != 0) {
                        xlog_error("wasi_instance_add_hostfd failed with %d",
                                   ret);
                        goto fail;
                }
        }
        ret = import_object_create_for_wasi(mctx, wasi, &wasi_import_object);
        if (ret != 0) {
                xlog_error("import_object_create_for_wasi failed with %d",
                           ret);
                goto fail;
        }

        /*
         * instantiate the module
         */
        struct report report;
        report_init(&report);
        wasi_import_object->next = base_imports;
        ret = instance_create(mctx, m, &inst, wasi_import_object, &report);
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
        struct exec_context ectx;
        exec_context_init(&ectx, inst, mctx);
        ret = instance_execute_func(&ectx, funcidx, pt, rt);
        ret = instance_execute_handle_restart(&ectx, ret);
        uint32_t wasi_exit_code = 0;
        if (ret == ETOYWASMTRAP) {
                const struct trap_info *trap = &ectx.trap;
                if (trap->trapid == TRAP_VOLUNTARY_EXIT) {
                        wasi_exit_code = wasi_instance_exit_code(wasi);
                } else {
                        xlog_error("got a trap %u: %s",
                                   (unsigned int)trap->trapid,
                                   report_getmessage(ectx.report));
                        exec_context_clear(&ectx);
                        goto fail;
                }
        } else if (ret != 0) {
                xlog_error("instance_execute_func failed with %d", ret);
                exec_context_clear(&ectx);
                goto fail;
        }
        exec_context_clear(&ectx);
        *wasi_exit_code_p = wasi_exit_code;
        ret = 0;
fail:
        /*
         * clean up
         */
        if (inst != NULL) {
                instance_destroy(inst);
        }
        if (wasi_import_object != NULL) {
                import_object_destroy(mctx, wasi_import_object);
        }
        if (wasi != NULL) {
                wasi_instance_destroy(wasi);
        }
        return ret;
}

int
runwasi(struct mem_context *mctx, const char *filename, unsigned int ndirs,
        char **dirs, unsigned int nenvs, const char *const *envs, int argc,
        const char *const *argv, const int stdio_fds[3],
        struct import_object *base_imports, uint32_t *wasi_exit_code_p)
{
        struct module *m = NULL;
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
        load_context_init(&lctx, mctx);
        ret = module_create(&m, p, p + sz, &lctx);
        if (ret != 0) {
                xlog_error("module_load failed with %d: %s", ret,
                           report_getmessage(&lctx.report));
                load_context_clear(&lctx);
                goto fail;
        }
        load_context_clear(&lctx);

        ret = runwasi_module(mctx, m, ndirs, dirs, nenvs, envs, argc, argv,
                             stdio_fds, base_imports, wasi_exit_code_p);

fail:
        if (m != NULL) {
                module_destroy(mctx, m);
        }
        if (p != NULL) {
                unmap_file(p, sz);
        }
        return ret;
}
