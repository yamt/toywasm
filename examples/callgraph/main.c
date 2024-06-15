#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <toywasm/fileio.h>
#include <toywasm/load_context.h>
#include <toywasm/mem.h>
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
        struct mem_context mctx;
        mem_context_init(&mctx);
        struct load_context ctx;
        load_context_init(&ctx, &mctx);
        ret = module_create(&m, p, p + sz, &ctx);
        if (ret != 0) {
                xlog_error("module_load failed with %d: %s", ret,
                           report_getmessage(&ctx.report));
                exit(1);
        }
        load_context_clear(&ctx);
        callgraph(m);
        module_destroy(&mctx, m);
        mem_context_clear(&mctx);
        exit(0);
}
