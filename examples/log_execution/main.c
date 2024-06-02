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

#include <toywasm/name.h>
#include <toywasm/xlog.h>

#include "log_execution.h"
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

        struct nametable table;
        nametable_init(&table);

        struct import_object *import_obj;
        ret = import_object_create_for_log_execution(&table, &import_obj);
        if (ret != 0) {
                exit(1);
        }
        ret = runwasi(a->filename, a->ndirs, a->dirs, a->nenvs,
                      (const char *const *)a->envs, a->argc,
                      (const char *const *)a->argv, stdio_fds, import_obj,
                      &wasi_exit_code);
        free(a->dirs);
        free(a->envs);
        nametable_clear(&table);
        if (ret != 0) {
                exit(1);
        }
        exit(wasi_exit_code);
}
