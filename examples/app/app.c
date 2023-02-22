#include <inttypes.h>
#include <stdlib.h>

#include <toywasm/fileio.h>
#include <toywasm/load_context.h>
#include <toywasm/module.h>
#include <toywasm/type.h>
#include <toywasm/xlog.h>

int
main(int argc, char **argv)
{
        xlog_printf("hello\n");
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
        ret = module_create(&m);
        if (ret != 0) {
                xlog_error("module_create failed with %d", ret);
                exit(1);
        }
        struct load_context ctx;
        load_context_init(&ctx);
        ret = module_load(m, p, p + sz, &ctx);
        if (ret != 0) {
                xlog_error("module_load failed with %d", ret);
                exit(1);
        }
        xlog_printf("module %s imports %" PRIu32 " functions\n", filename,
                    m->nimportedfuncs);
        xlog_printf("module %s contains %" PRIu32 " functions\n", filename,
                    m->nfuncs);
        exit(0);
}
