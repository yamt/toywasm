#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <toywasm/fileio.h>
#include <toywasm/load_context.h>
#include <toywasm/module.h>
#include <toywasm/xlog.h>

#include "callgraph.h"

int
main(int argc, char **argv)
{
        if (argc != 2) {
                xlog_error("unexpected number of args");
                exit(2);
        }
        const char *filename = argv[1];
        struct module *m;
        int ret;
        uint8_t *p;
        size_t sz;
        ret = map_file(filename, (void **)&p, &sz);
        if (ret != 0) {
                xlog_error("map_file failed with %d", ret);
                exit(1);
        }
        struct load_context ctx;
        load_context_init(&ctx);
        ret = module_create(&m, p, p + sz, &ctx);
        if (ret != 0) {
                xlog_error("module_load failed with %d: %s", ret,
                           report_getmessage(&ctx.report));
                exit(1);
        }
        callgraph(m);
        exit(0);
}
