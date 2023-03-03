#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repl.h"
#include "toywasm_config.h"
#if defined(TOYWASM_ENABLE_WASI_THREADS)
#include "wasi_threads.h"
#endif
#include "xlog.h"

enum longopt {
        opt_disable_jump_table = 0x100,
        opt_disable_localtype_cellidx,
        opt_disable_resulttype_cellidx,
        opt_invoke,
        opt_load,
        opt_max_frames,
        opt_max_stack_cells,
        opt_register,
        opt_repl,
        opt_repl_prompt,
        opt_print_stats,
        opt_trace,
        opt_version,
        opt_wasi,
        opt_wasi_dir,
        opt_wasi_mapdir,
        opt_wasi_env,
};

const struct option longopts[] = {
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
                "print-stats",
                no_argument,
                NULL,
                opt_print_stats,
        },
        {
                "trace",
                required_argument,
                NULL,
                opt_trace,
        },
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
                "wasi-mapdir",
                required_argument,
                NULL,
                opt_wasi_mapdir,
        },
        {
                "wasi-env",
                required_argument,
                NULL,
                opt_wasi_env,
        },
#endif
        {
                NULL,
                0,
                NULL,
                0,
        },
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
                switch (opt->has_arg) {
                case no_argument:
                        printf("\t--%s\n", opt->name);
                        break;
                case required_argument:
                        printf("\t--%s ARG\n", opt->name);
                        break;
                case optional_argument:
                        printf("\t--%s [ARG]\n", opt->name);
                        break;
                }
                opt++;
        }
}

int
main(int argc, char *const *argv)
{
        struct repl_state *state;
#if defined(TOYWASM_ENABLE_WASI)
        int nenvs = 0;
        char **envs = NULL;
#endif
        int ret;
        int longidx;
        bool do_repl = false;
        bool might_need_help = true;

        int exit_status = 1;

#if defined(__NuttX__)
        xlog_tracing = 0;
#endif

        state = malloc(sizeof(*state));
        if (state == NULL) {
                goto fail;
        }
        toywasm_repl_state_init(state);
        struct repl_options *opts = &state->opts;
        while ((ret = getopt_long(argc, argv, "", longopts, &longidx)) != -1) {
                switch (ret) {
                case opt_disable_jump_table:
                        opts->load_options.generate_jump_table = false;
                        break;
                case opt_disable_localtype_cellidx:
                        opts->load_options.generate_localtype_cellidx = false;
                        break;
                case opt_disable_resulttype_cellidx:
                        opts->load_options.generate_resulttype_cellidx = false;
                        break;
                case opt_invoke:
                        ret = toywasm_repl_invoke(state, NULL, optarg, NULL,
                                                  true);
                        if (ret != 0) {
                                goto fail;
                        }
                        might_need_help = false;
                        break;
                case opt_load:
                        ret = toywasm_repl_load(state, NULL, optarg);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
                case opt_max_frames:
                        opts->exec_options.max_frames = atoi(optarg);
                        break;
                case opt_max_stack_cells:
                        opts->exec_options.max_stackcells = atoi(optarg);
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
                case opt_print_stats:
                        opts->print_stats = true;
                        break;
                case opt_trace:
                        xlog_tracing = atoi(optarg);
                        break;
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
                                goto fail;
                        }
                        break;
                case opt_wasi_mapdir:
                        ret = toywasm_repl_set_wasi_prestat_mapdir(state,
                                                                   optarg);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
                case opt_wasi_env:
                        nenvs++;
                        envs = realloc(envs, nenvs * sizeof(*envs));
                        if (envs == NULL) {
                                ret = ENOMEM;
                                goto fail;
                        }
                        envs[nenvs - 1] = optarg;
                        ret = toywasm_repl_set_wasi_environ(state, nenvs,
                                                            envs);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
#endif
                default:
                        print_usage();
                        exit(0);
                }
        }
        argc -= optind;
        argv += optind;

        if (do_repl) {
                ret = toywasm_repl(state);
                if (ret != 0) {
                        exit(1);
                }
                exit(0);
        }
        if (argc == 0) {
                if (might_need_help) {
                        print_usage();
                }
                exit(0);
        }
#if defined(TOYWASM_ENABLE_WASI)
        ret = toywasm_repl_set_wasi_args(state, argc, argv);
        if (ret != 0 && ret != EPROTO) {
                goto fail;
        }
#endif
        const char *filename = argv[0];
        ret = toywasm_repl_load(state, NULL, filename);
        if (ret != 0) {
                xlog_error("load failed");
                goto fail;
        }
#if defined(TOYWASM_ENABLE_WASI_THREADS)
        if (state->wasi_threads != NULL) {
                const struct repl_module_state *mod =
                        &state->modules[state->nmodules - 1];
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
                goto fail;
        }
#if defined(TOYWASM_ENABLE_WASI)
        exit_status = wasi_exit_code;
#endif
fail:
        toywasm_repl_reset(state);
#if defined(TOYWASM_ENABLE_WASI)
        free(envs);
#endif
        free(state);
        exit(exit_status);
}
