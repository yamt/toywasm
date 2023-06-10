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
#include <unistd.h>

#include <toywasm/xlog.h>

#include "runwasi.h"

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
        uint32_t wasi_exit_code;
        const int stdio_fds[3] = {
                STDIN_FILENO,
                STDOUT_FILENO,
                STDERR_FILENO,
        };
        ret = runwasi(filename, ndirs, dirs, nenvs, envs, argc, argv,
                      stdio_fds, &wasi_exit_code);
        free(dirs);
        free(envs);
        if (ret != 0) {
                exit(1);
        }
        exit(wasi_exit_code);
}
