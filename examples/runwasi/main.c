/*
 * an example app to run a wasi command.
 *
 * usage:
 * % runwasi --dir=. --env=a=b -- foo.wasm -x -y
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <toywasm/mem.h>
#include <toywasm/xlog.h>

#include "runwasi.h"
#include "runwasi_cli_args.h"

int
main(int argc, char **argv)
{
        struct runwasi_cli_args a0;
        struct runwasi_cli_args *a = &a0;
        uint32_t wasi_exit_code;
        int ret;
        const int stdio_fds[3] = {
                STDIN_FILENO,
                STDOUT_FILENO,
                STDERR_FILENO,
        };
        ret = runwasi_cli_args_parse(argc, argv, a);
        if (ret != 0) {
                xlog_error("failed to process cli arguments");
                exit(1);
        }
        struct mem_context mctx;
        mem_context_init(&mctx);
        ret = runwasi(&mctx, a->filename, a->ndirs, a->dirs, a->nenvs,
                      (const char *const *)a->envs, a->argc,
                      (const char *const *)a->argv, stdio_fds, NULL, NULL,
                      NULL, &wasi_exit_code);
        mem_context_clear(&mctx);
        free(a->dirs);
        free(a->envs);
        if (ret != 0) {
                exit(1);
        }
        exit(wasi_exit_code);
}
