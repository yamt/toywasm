#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repl.h"
#include "xlog.h"

enum longopt {
        opt_disable_jump_table = 0x100,
        opt_invoke,
        opt_load,
        opt_register,
        opt_repl,
        opt_repl_prompt,
        opt_print_stats,
        opt_trace,
        opt_version,
        opt_wasi,
        opt_wasi_dir,
};

const struct option longopts[] = {
        {
                "disable-jump-table",
                no_argument,
                NULL,
                opt_disable_jump_table,
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
                "register",
                required_argument,
                NULL,
                opt_register,
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
                no_argument,
                NULL,
                opt_trace,
        },
        {
                "version",
                no_argument,
                NULL,
                opt_version,
        },
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
                NULL,
                0,
                NULL,
                0,
        },
};

int
main(int argc, char *const *argv)
{
        struct repl_state state0;
        struct repl_state *state = &state0;
        int ret;
        int longidx;
        bool do_repl = false;

        int exit_status = 1;

        repl_state_init(state);
        struct repl_options *opts = &state->opts;
        while ((ret = getopt_long(argc, argv, "", longopts, &longidx)) != -1) {
                switch (ret) {
                case opt_disable_jump_table:
                        opts->load_options.generate_jump_table = false;
                        break;
                case opt_invoke:
                        ret = repl_invoke(state, NULL, optarg, NULL, true);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
                case opt_load:
                        ret = repl_load(state, NULL, optarg);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
                case opt_register:
                        ret = repl_register(state, NULL, optarg);
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
                        xlog_tracing = 1;
                        break;
                case opt_version:
                        repl_print_version();
                        break;
                case opt_wasi:
                        ret = repl_load_wasi(state);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
                case opt_wasi_dir:
                        ret = repl_set_wasi_prestat(state, optarg);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
                }
        }
        argc -= optind;
        argv += optind;

        if (do_repl) {
                ret = repl(state);
                if (ret != 0) {
                        exit(1);
                }
                exit(0);
        }
        if (argc == 0) {
                exit(0);
        }
        ret = repl_set_wasi_args(state, argc, argv);
        if (ret != 0 && ret != EPROTO) {
                goto fail;
        }
        const char *filename = argv[0];
        ret = repl_load(state, NULL, filename);
        if (ret != 0) {
                xlog_error("load failed");
                goto fail;
        }
        uint32_t wasi_exit_code = 0;
        ret = repl_invoke(state, NULL, "_start", &wasi_exit_code, false);
        if (ret != 0) {
                xlog_error("invoke failed with %d", ret);
                goto fail;
        }
        exit_status = wasi_exit_code;
fail:
        repl_reset(state);
        exit(exit_status);
}
