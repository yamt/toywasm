
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

#include "runwasi_cli_args.h"

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
runwasi_cli_args_parse(int argc, char **argv, struct runwasi_cli_args *a)
{
        unsigned int nenvs = 0;
        char **envs = NULL;
        unsigned int ndirs = 0;
        char **dirs = NULL;
        void *p;

        int ret;
        int longidx;
        while ((ret = getopt_long(argc, argv, "", longopts, &longidx)) != -1) {
                switch (ret) {
                case opt_wasi_dir:
                        ndirs++;
                        p = realloc(dirs, ndirs * sizeof(*dirs));
                        if (p == NULL) {
                                goto fail;
                        }
                        dirs = p;
                        dirs[ndirs - 1] = optarg;
                        break;
                case opt_wasi_env:
                        nenvs++;
                        p = realloc(envs, nenvs * sizeof(*envs));
                        if (p == NULL) {
                                goto fail;
                        }
                        envs = p;
                        envs[nenvs - 1] = optarg;
                        break;
                default:
                        goto fail;
                }
        }
        argc -= optind;
        argv += optind;
        if (argc < 1) {
                goto fail;
        }
        const char *filename = argv[0];

        a->filename = filename;
        a->ndirs = ndirs;
        a->dirs = dirs;
        a->nenvs = nenvs;
        a->envs = envs;
        a->argc = argc;
        a->argv = argv;
        return 0;
fail:
        free(envs);
        free(dirs);
        return 1;
}
