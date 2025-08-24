#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "repl.h"
#include "str_to_uint.h"
#include "toywasm_config.h"
#if defined(TOYWASM_ENABLE_WASI_THREADS)
#include "wasi_threads.h"
#endif
#if defined(TOYWASM_ENABLE_WASI_LITTLEFS)
#include "wasi_littlefs.h"
#endif
#include "vec.h"
#include "xlog.h"

enum longopt {
        opt_allow_unresolved_functions = 0x100,
        opt_disable_jump_table,
        opt_disable_localtype_cellidx,
        opt_disable_resulttype_cellidx,
#if defined(TOYWASM_ENABLE_DYLD)
        opt_dyld,
        opt_dyld_bindnow,
#if defined(TOYWASM_ENABLE_DYLD_DLFCN)
        opt_dyld_dlfcn,
#endif
        opt_dyld_path,
        opt_dyld_stack_size,
#endif
        opt_invoke,
        opt_load,
        opt_max_frames,
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
        opt_max_memory,
#endif
        opt_max_stack_cells,
        opt_register,
        opt_repl,
        opt_repl_prompt,
        opt_print_build_options,
        opt_print_stats,
        opt_timeout,
#if defined(TOYWASM_ENABLE_TRACING)
        opt_trace,
#endif
        opt_version,
#if defined(TOYWASM_ENABLE_WASI)
        opt_wasi,
        opt_wasi_dir,
        opt_wasi_env,
#endif
#if defined(TOYWASM_ENABLE_WASI_LITTLEFS)
        opt_wasi_littlefs_dir,
        opt_wasi_littlefs_block_size,
        opt_wasi_littlefs_disk_version,
#endif
};

static const struct option longopts[] = {
        {
                "allow-unresolved-functions",
                no_argument,
                NULL,
                opt_allow_unresolved_functions,
        },
        {
                "disable-jump-table",
                no_argument,
                NULL,
                opt_disable_jump_table,
        },
        {
                "disable-localtype-cellidx",
                no_argument,
                NULL,
                opt_disable_localtype_cellidx,
        },
        {
                "disable-resulttype-cellidx",
                no_argument,
                NULL,
                opt_disable_resulttype_cellidx,
        },
#if defined(TOYWASM_ENABLE_DYLD)
        {
                "dyld",
                no_argument,
                NULL,
                opt_dyld,
        },
        {
                "dyld-bindnow",
                no_argument,
                NULL,
                opt_dyld_bindnow,
        },
#if defined(TOYWASM_ENABLE_DYLD_DLFCN)
        {
                "dyld-dlfcn",
                no_argument,
                NULL,
                opt_dyld_dlfcn,
        },
#endif
        {
                "dyld-path",
                required_argument,
                NULL,
                opt_dyld_path,
        },
        {
                "dyld-stack-size",
                required_argument,
                NULL,
                opt_dyld_stack_size,
        },
#endif
        {
                "invoke",
                required_argument,
                NULL,
                opt_invoke,
        },
        {
                "load",
                required_argument,
                NULL,
                opt_load,
        },
        {
                "max-frames",
                required_argument,
                NULL,
                opt_max_frames,
        },
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
        {
                "max-memory",
                required_argument,
                NULL,
                opt_max_memory,
        },
#endif
        {
                "max-stack-cells",
                required_argument,
                NULL,
                opt_max_stack_cells,
        },
        {
                "repl",
                no_argument,
                NULL,
                opt_repl,
        },
        {
                "repl-prompt",
                required_argument,
                NULL,
                opt_repl_prompt,
        },
        {
                "print-build-options",
                no_argument,
                NULL,
                opt_print_build_options,
        },
        {
                "print-stats",
                no_argument,
                NULL,
                opt_print_stats,
        },
        {
                "timeout",
                required_argument,
                NULL,
                opt_timeout,
        },
#if defined(TOYWASM_ENABLE_TRACING)
        {
                "trace",
                required_argument,
                NULL,
                opt_trace,
        },
#endif
        {
                "version",
                no_argument,
                NULL,
                opt_version,
        },
#if defined(TOYWASM_ENABLE_WASI)
        {
                "wasi",
                no_argument,
                NULL,
                opt_wasi,
        },
        {
                "wasi-dir",
                required_argument,
                NULL,
                opt_wasi_dir,
        },
        {
                "wasi-env",
                required_argument,
                NULL,
                opt_wasi_env,
        },
#endif
#if defined(TOYWASM_ENABLE_WASI_LITTLEFS)
        {
                "wasi-littlefs-dir",
                required_argument,
                NULL,
                opt_wasi_littlefs_dir,
        },
        {
                "wasi-littlefs-block-size",
                required_argument,
                NULL,
                opt_wasi_littlefs_block_size,
        },
        {
                "wasi-littlefs-disk-version",
                required_argument,
                NULL,
                opt_wasi_littlefs_disk_version,
        },
#endif
        {
                NULL,
                0,
                NULL,
                0,
        },
};

static const char *opt_metavars[] = {
        [opt_invoke] = "FUNCTION[ FUNCTION_ARGS...]",
        [opt_load] = "MODULE_PATH",
#if defined(TOYWASM_ENABLE_DYLD)
        [opt_dyld_path] = "LIBRARY_DIR",
        [opt_dyld_stack_size] = "C_STACK_SIZE_FOR_PIE_IN_BYTES",
#endif
#if defined(TOYWASM_ENABLE_WASI)
        [opt_wasi_env] = "NAME=VAR",
        [opt_wasi_dir] = "HOST_DIR[::GUEST_DIR]",
#endif
#if defined(TOYWASM_ENABLE_WASI_LITTLEFS)
        [opt_wasi_littlefs_dir] = "LITTLEFS_IMAGE_PATH::LFS_DIR[::GUEST_DIR]",
        [opt_wasi_littlefs_block_size] = "BLOCK_SIZE",
        [opt_wasi_littlefs_disk_version] = "DISK_VERSION",
#endif
        [opt_timeout] = "TIMEOUT_MS",
#if defined(TOYWASM_ENABLE_TRACING)
        [opt_trace] = "LEVEL",
#endif
        [opt_repl_prompt] = "STRING",
        [opt_max_frames] = "NUMBER_OF_FRAMES",
        [opt_max_stack_cells] = "NUMBER_OF_CELLS",
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
        [opt_max_memory] = "MEMORY_LIMIT_IN_BYTES",
#endif
};

static void
print_usage(void)
{
        printf("Usage:\n");
#if defined(TOYWASM_ENABLE_WASI)
        printf("\ttoywasm [OPTIONS] [--] <MODULE> [WASI-ARGS...]\n");
#else
        printf("\ttoywasm [OPTIONS] [--] <MODULE>\n");
#endif

        printf("Options:\n");
        const struct option *opt = longopts;
        while (opt->name != NULL) {
                const char *metavar = NULL;
                if (opt->val < ARRAYCOUNT(opt_metavars)) {
                        metavar = opt_metavars[opt->val];
                }
                if (metavar == NULL) {
                        metavar = "ARG";
                }
                switch (opt->has_arg) {
                case no_argument:
                        printf("\t--%s\n", opt->name);
                        break;
                case required_argument:
                        printf("\t--%s %s\n", opt->name, metavar);
                        break;
                case optional_argument:
                        printf("\t--%s [%s]\n", opt->name, metavar);
                        break;
                }
                opt++;
        }
        printf("Examples:\n");
#if defined(TOYWASM_ENABLE_WASI)
        printf("\tRun a wasi module\n\t\ttoywasm --wasi module\n");
#endif
        printf("\tLoad a module and invoke its function\n\t\ttoywasm --load "
               "module --invoke \"func arg1 arg2\"\n");
}

int
main(int argc, char **argv)
{
        struct mem_context mctx0, *mctx = &mctx0;
        struct mem_context modules_mctx0, *modules_mctx = &modules_mctx0;
        struct mem_context instances_mctx0, *instances_mctx = &instances_mctx0;
        struct mem_context wasi_mctx0, *wasi_mctx = &wasi_mctx0;
        struct mem_context dyld_mctx0, *dyld_mctx = &dyld_mctx0;
        struct mem_context impobj_mctx0, *impobj_mctx = &impobj_mctx0;

        struct repl_state *state;
#if defined(TOYWASM_ENABLE_WASI)
        VEC(, const char *) wasi_envs;
        VEC_INIT(wasi_envs);
#endif
#if defined(TOYWASM_ENABLE_DYLD)
        VEC(, const char *) dyld_paths;
        VEC_INIT(dyld_paths);
#endif
        int ret;
        int longidx;
        bool do_repl = false;
        bool might_need_help = true;

        int exit_status = 1;

#if defined(__NuttX__) && defined(CONFIG_BUILD_FLAT)
        /*
         * in nuttx flat model, bss/data is shared among tasks.
         *
         * we only have a few mutable non-heap data in toywasm:
         *
         * - xlog_tracing, which we bluntly reset here, somehow assuming
         *   a single user. hopefully it won't be a big problem as tracing
         *   is rarely enabled. otherwise, we might want to introduce a
         *   "xlog_context" or something along the line.
         *
         * - the "x" counter for interrupt_debug, which is ok with any values.
         */
#if defined(TOYWASM_ENABLE_TRACING)
        xlog_tracing = 0;
#endif
#endif
        mem_context_init(mctx);
        mem_context_init(modules_mctx);
        mem_context_init(instances_mctx);
        mem_context_init(wasi_mctx);
        mem_context_init(dyld_mctx);
        mem_context_init(impobj_mctx);
        modules_mctx->parent = mctx;
        instances_mctx->parent = mctx;
        wasi_mctx->parent = mctx;
        dyld_mctx->parent = mctx;
        impobj_mctx->parent = mctx;

        state = malloc(sizeof(*state));
        if (state == NULL) {
                goto fail;
        }
        toywasm_repl_state_init(state);
        state->mctx = mctx;
        state->modules_mctx = modules_mctx;
        state->instances_mctx = instances_mctx;
        state->wasi_mctx = wasi_mctx;
        state->dyld_mctx = dyld_mctx;
        state->impobj_mctx = impobj_mctx;
        struct repl_options *opts = &state->opts;
        size_t limit;
        while ((ret = getopt_long(argc, argv, "", longopts, &longidx)) != -1) {
                switch (ret) {
                case opt_allow_unresolved_functions:
                        opts->allow_unresolved_functions = true;
                        break;
                case opt_disable_jump_table:
                        opts->load_options.generate_jump_table = false;
                        break;
                case opt_disable_localtype_cellidx:
#if defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
                        opts->load_options.generate_localtype_cellidx = false;
#endif
                        break;
                case opt_disable_resulttype_cellidx:
#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
                        opts->load_options.generate_resulttype_cellidx = false;
#endif
                        break;
#if defined(TOYWASM_ENABLE_DYLD)
                case opt_dyld:
                        if (state->modules.lsize > 0) {
                                ret = EPROTO;
                                goto fail;
                        }
                        opts->enable_dyld = true;
                        break;
                case opt_dyld_bindnow:
                        opts->dyld_options.bindnow = true;
                        break;
#if defined(TOYWASM_ENABLE_DYLD_DLFCN)
                case opt_dyld_dlfcn:
                        opts->dyld_options.enable_dlfcn = true;
                        break;
#endif
                case opt_dyld_path:
                        ret = VEC_PREALLOC(mctx, dyld_paths, 1);
                        if (ret != 0) {
                                goto fail;
                        }
                        *VEC_PUSH(dyld_paths) = optarg;
                        opts->dyld_options.npaths = dyld_paths.lsize;
                        opts->dyld_options.paths = dyld_paths.p;
                        break;
                case opt_dyld_stack_size:
                        ret = str_to_u32(optarg, 0,
                                         &opts->dyld_options.stack_size);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
#endif
                case opt_invoke:
                        ret = toywasm_repl_invoke(state, NULL, optarg, NULL,
                                                  true);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
                case opt_load:
                        ret = toywasm_repl_load(state, NULL, optarg, false);
                        if (ret != 0) {
                                goto fail;
                        }
                        might_need_help = false;
                        break;
                case opt_max_frames:
                        ret = str_to_u32(optarg, 0,
                                         &opts->exec_options.max_frames);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
                case opt_max_memory:
                        ret = str_to_size(optarg, 0, &limit);
                        if (ret != 0) {
                                goto fail;
                        }
                        ret = mem_context_setlimit(mctx, limit);
                        if (ret != 0) {
                                xlog_error("failed to set limit %zu (current "
                                           "allocation: %zu)",
                                           limit, mctx->allocated);
                                goto fail;
                        }
                        break;
#endif
                case opt_max_stack_cells:
                        ret = str_to_u32(optarg, 0,
                                         &opts->exec_options.max_stackcells);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
                case opt_register:
                        ret = toywasm_repl_register(state, NULL, optarg);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
                case opt_repl:
                        do_repl = true;
                        break;
                case opt_repl_prompt:
                        opts->prompt = optarg;
                        break;
                case opt_print_build_options:
                        toywasm_repl_print_build_options();
                        might_need_help = false;
                        break;
                case opt_print_stats:
                        opts->print_stats = true;
                        break;
                case opt_timeout:
                        toywasm_repl_set_timeout(state, atoi(optarg));
                        break;
#if defined(TOYWASM_ENABLE_TRACING)
                case opt_trace:
                        xlog_tracing = atoi(optarg);
                        break;
#endif
                case opt_version:
                        toywasm_repl_print_version();
                        might_need_help = false;
                        break;
#if defined(TOYWASM_ENABLE_WASI)
                case opt_wasi:
                        ret = toywasm_repl_load_wasi(state);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
                case opt_wasi_dir:
                        ret = toywasm_repl_set_wasi_prestat(state, optarg);
                        if (ret != 0) {
                                xlog_error(
                                        "failed to add preopen '%s' error %d",
                                        optarg, ret);
                                goto fail;
                        }
                        break;
                case opt_wasi_env:
                        ret = VEC_PREALLOC(mctx, wasi_envs, 1);
                        if (ret != 0) {
                                goto fail;
                        }
                        *VEC_PUSH(wasi_envs) = optarg;
                        ret = toywasm_repl_set_wasi_environ(
                                state, wasi_envs.lsize, wasi_envs.p);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
#endif
#if defined(TOYWASM_ENABLE_WASI_LITTLEFS)
                case opt_wasi_littlefs_dir:
                        ret = toywasm_repl_set_wasi_prestat_littlefs(state,
                                                                     optarg);
                        if (ret != 0) {
                                xlog_error(
                                        "failed to add preopen '%s' error %d",
                                        optarg, ret);
                                goto fail;
                        }
                        break;
                case opt_wasi_littlefs_block_size:
                        ret = str_to_u32(
                                optarg, 0,
                                &opts->wasi_littlefs_mount_cfg.block_size);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
                case opt_wasi_littlefs_disk_version:
                        ret = str_to_u32(
                                optarg, 0,
                                &opts->wasi_littlefs_mount_cfg.disk_version);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
#endif
                default:
                        print_usage();
                        goto fail;
                }
        }
        argc -= optind;
        argv += optind;

        if (do_repl) {
                ret = toywasm_repl(state);
                if (ret != 0) {
                        goto fail;
                }
                goto success;
        }
        if (argc == 0) {
                if (might_need_help) {
                        print_usage();
                }
                goto success;
        }
        const char *filename = argv[0];
#if defined(TOYWASM_ENABLE_WASI)
        /*
         * sanitize the module filename before passing it to wasi to
         * avoid leaking the information about the host path.
         *
         * note: unfortunately wasm runtime implementations out there
         * are incompatible on this. this even causes incompatible
         * behaviors with real applications. eg. cpython uses it to
         * search associated files.
         *
         * as of writing this,
         *
         *   only use the basename or something along the line:
         *     toywasm
         *     wasmtime
         *     wasmi_cli
         *     wazero
         *     wasm3
         *
         *   use the given path as it is:
         *     wasmer
         *     wasm-micro-runtime
         */
        char *slash = strrchr(argv[0], '/');
        if (slash != NULL) {
                if (slash[1] == 0) {
                        xlog_error("module filename ends with a slash");
                        goto fail;
                }
                argv[0] = slash + 1;
        }
        ret = toywasm_repl_set_wasi_args(state, argc, (const char **)argv);
        if (ret != 0 && ret != EPROTO) {
                goto fail;
        }
#endif
        ret = toywasm_repl_load(state, NULL, filename, false);
        if (ret != 0) {
                xlog_error("load failed");
                goto fail;
        }
#if defined(TOYWASM_ENABLE_WASI_THREADS)
        if (state->wasi_threads != NULL
#if defined(TOYWASM_ENABLE_DYLD)
            /* if dyld is enabled, make thread_spawn fail. */
            && !opts->enable_dyld
#endif
        ) {
                const struct repl_module_state_u *mod_u =
                        &VEC_LASTELEM(state->modules);
                const struct repl_module_state *mod = &mod_u->u.repl;
                ret = wasi_threads_instance_set_thread_spawn_args(
                        state->wasi_threads, mod->module, mod->extra_import);
                if (ret != 0) {
                        xlog_error("wasi_threads_instance_set_thread_spawn_"
                                   "args failed with %d",
                                   ret);
                        goto fail;
                }
        }
#endif
        uint32_t wasi_exit_code = 0;
        ret = toywasm_repl_invoke(state, NULL, "_start", &wasi_exit_code,
                                  false);
        if (ret != 0) {
                xlog_error("invoke failed with %d", ret);
                /*
                 * REVISIT: should we use a distinguishable exit code
                 * for ETOYWASMTRAP?
                 *
                 * Note: the exit code used for a "unreachable"
                 * trap varies among runtimes:
                 *
                 *   toywasm   1
                 *   wasm3     1
                 *   wamr      1
                 *   wasmer    128+SIGABRT on unix
                 *   wasmtime  128+SIGABRT on unix
                 *   wazero    0 (!)
                 *   wasmi_cli 1
                 *
                 * Note: wasmtime traps when wasi proc_exit is
                 * called with exit code >=126. in this case,
                 * wasmtime exits with 1.
                 * (Also, see the comment in libwasi wasi_proc_exit.)
                 *
                 * Thus, if we make toywasm return 128+SIGABRT on a trap
                 * and if you run toywasm on wasmtime, wasmtime rejects
                 * toywasm's exit code and exits with 1.
                 *
                 * Probably the idea to represent both of wasi exit code
                 * and other exit reasons like a trap with a single exit
                 * code is broken by design.
                 */
                goto fail;
        }
#if defined(TOYWASM_ENABLE_WASI)
        exit_status = wasi_exit_code;
#else
        exit_status = 0;
#endif
fail:
        toywasm_repl_reset(state);
#if defined(TOYWASM_ENABLE_WASI)
        VEC_FREE(mctx, wasi_envs);
#endif
#if defined(TOYWASM_ENABLE_DYLD)
        VEC_FREE(mctx, dyld_paths);
#endif
        free(state);
        mem_context_clear(dyld_mctx);
        mem_context_clear(wasi_mctx);
        mem_context_clear(instances_mctx);
        mem_context_clear(modules_mctx);
        mem_context_clear(mctx);
        exit(exit_status);
success:
        exit_status = 0;
        goto fail;
}
