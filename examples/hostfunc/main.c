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

#include <toywasm/host_instance.h>
#include <toywasm/instance.h>
#include <toywasm/mem.h>
#include <toywasm/xlog.h>

#include "hostfunc.h"
#include "runwasi.h"
#include "runwasi_cli_args.h"

static void
set_resources(void *v, struct meminst *mem, struct tableinst *func_table)
{
        struct host_instance *hi = v;
        hi->memory = mem;
        hi->func_table = func_table;
}

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
        struct host_instance hi;
        hi.memory = NULL;
        hi.func_table = NULL;
        struct import_object *import_obj;
        ret = import_object_create_for_my_host_inst(&mctx, &hi, &import_obj);
        if (ret != 0) {
                exit(1);
        }
        ret = runwasi(&mctx, a->filename, a->ndirs, a->dirs, a->nenvs,
                      (const char *const *)a->envs, a->argc,
                      (const char *const *)a->argv, stdio_fds, import_obj,
                      set_resources, &hi, &wasi_exit_code);
        import_object_destroy(&mctx, import_obj);
        mem_context_clear(&mctx);
        free(a->dirs);
        free(a->envs);
        if (ret != 0) {
                exit(1);
        }
        exit(wasi_exit_code);
}
