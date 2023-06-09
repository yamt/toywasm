/*
 * an example app to run a wasi command.
 *
 * usage:
 * % runwasi --dir=. --env=a=b -- foo.wasm -x -y
 */

#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <toywasm/exec_context.h>
#include <toywasm/fileio.h>
#include <toywasm/instance.h>
#include <toywasm/load_context.h>
#include <toywasm/module.h>
#include <toywasm/report.h>
#include <toywasm/type.h>
#include <toywasm/wasi.h>
#include <toywasm/xlog.h>

enum longopt {
        opt_wasi_dir,
        opt_wasi_env,
};

static const struct option longopts[] = {
        {
                "dir",
                required_argument,
                NULL,
                opt_wasi_dir,
        },
        {
                "env",
                required_argument,
                NULL,
                opt_wasi_env,
        },
        {
                NULL,
                0,
                NULL,
                0,
        },
};

int
main(int argc, char **argv)
{
        unsigned int nenvs = 0;
        char **envs = NULL;
        unsigned int ndirs = 0;
        char **dirs = NULL;

        int ret;
        int longidx;
        while ((ret = getopt_long(argc, argv, "", longopts, &longidx)) != -1) {
                switch (ret) {
                case opt_wasi_dir:
                        ndirs++;
                        dirs = realloc(dirs, ndirs * sizeof(*dirs));
                        if (dirs == NULL) {
                                xlog_error("realloc failed");
                                exit(1);
                        }
                        dirs[ndirs - 1] = optarg;
                        break;
                case opt_wasi_env:
                        nenvs++;
                        envs = realloc(envs, nenvs * sizeof(*envs));
                        if (envs == NULL) {
                                xlog_error("realloc failed");
                                exit(1);
                        }
                        envs[nenvs - 1] = optarg;
                        break;
                default:
                        exit(1);
                }
        }
        argc -= optind;
        argv += optind;
        if (argc < 1) {
                xlog_error("unexpected number of args");
                exit(2);
        }
        const char *filename = argv[0];
        int wasi_argc = argc;
        char **wasi_argv = argv;

        /*
         * load a module
         */
        struct module *m;
        uint8_t *p;
        size_t sz;
        ret = map_file(filename, (void **)&p, &sz);
        if (ret != 0) {
                xlog_error("map_file failed with %d", ret);
                exit(1);
        }
        struct load_context lctx;
        load_context_init(&lctx);
        ret = module_create(&m, p, p + sz, &lctx);
        if (ret != 0) {
                xlog_error("module_load failed with %d", ret);
                exit(1);
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
                exit(1);
        }
        const struct functype *ft = module_functype(m, funcidx);
        const struct resulttype *pt = &ft->parameter;
        const struct resulttype *rt = &ft->result;
        if (pt->ntypes != 0 || rt->ntypes != 0) {
                xlog_error("unexpected type of _start");
                exit(1);
        }

        /*
         * create a wasi instance
         */
        struct wasi_instance *wasi;
        ret = wasi_instance_create(&wasi);
        if (ret != 0) {
                xlog_error("wasi_instance_create failed with %d", ret);
                exit(1);
        }
        wasi_instance_set_args(wasi, wasi_argc, wasi_argv);
        wasi_instance_set_environ(wasi, nenvs, envs);
        unsigned int i;
        for (i = 0; i < ndirs; i++) {
                ret = wasi_instance_prestat_add(wasi, dirs[i]);
                if (ret != 0) {
                        xlog_error("wasi_instance_prestat_add failed with %d",
                                   ret);
                        exit(1);
                }
        }
        free(dirs);
        struct import_object *wasi_import_object;
        ret = import_object_create_for_wasi(wasi, &wasi_import_object);
        if (ret != 0) {
                xlog_error("import_object_create_for_wasi failed with %d",
                           ret);
                exit(1);
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
                exit(1);
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
                        exit(1);
                }
        } else if (ret != 0) {
                xlog_error("instance_execute_func failed with %d", ret);
                exit(1);
        }
        exec_context_clear(&ectx);

        /*
         * clean up
         */
        instance_destroy(inst);
        import_object_destroy(wasi_import_object);
        wasi_instance_destroy(wasi);
        module_destroy(m);
        unmap_file(p, sz);

        exit(wasi_exit_code);
}
