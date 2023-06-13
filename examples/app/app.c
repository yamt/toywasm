#include <inttypes.h>
#include <stdio.h>
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
        struct load_context ctx;
        load_context_init(&ctx);
        ret = module_create(&m, p, p + sz, &ctx);
        if (ret != 0) {
                xlog_error("module_load failed with %d", ret);
                exit(1);
        }
        xlog_printf("module %s imports %" PRIu32 " functions\n", filename,
                    m->nimportedfuncs);
        xlog_printf("module %s contains %" PRIu32 " functions\n", filename,
                    m->nfuncs);
        module_print_stats(m);

        /* perform some investigations on the loaded module */
        uint32_t i;
        uint32_t funcidx_with_most_locals = UINT32_MAX;
        uint32_t nlocals = 0;
        uint32_t funcidx_with_most_params = UINT32_MAX;
        uint32_t nparams = 0;
        uint32_t funcidx_with_most_results = UINT32_MAX;
        uint32_t nresults = 0;
#if defined(TOYWASM_ENABLE_WRITER)
        uint32_t funcidx_with_max_code_size = UINT32_MAX;
        uint32_t max_code_size = 0;
#endif
        for (i = 0; i < m->nfuncs - m->nimportedfuncs; i++) {
                const struct func *func = &m->funcs[i];
                const struct localtype *lt = &func->localtype;
                if (funcidx_with_most_locals == UINT32_MAX ||
                    lt->nlocals > nlocals) {
                        funcidx_with_most_locals = i;
                        nlocals = lt->nlocals;
                }
                const struct functype *ft = &m->types[m->functypeidxes[i]];
                if (funcidx_with_most_params == UINT32_MAX ||
                    ft->parameter.ntypes > nparams) {
                        funcidx_with_most_params = i;
                        nparams = ft->parameter.ntypes;
                }
                if (funcidx_with_most_results == UINT32_MAX ||
                    ft->result.ntypes > nresults) {
                        funcidx_with_most_results = i;
                        nresults = ft->result.ntypes;
                }
#if defined(TOYWASM_ENABLE_WRITER)
                uint32_t code_size = func->e.end - func->e.start;
                if (funcidx_with_max_code_size == UINT32_MAX ||
                    code_size > max_code_size) {
                        funcidx_with_max_code_size = i;
                        max_code_size = code_size;
                }
#endif
        }
        if (funcidx_with_most_locals != UINT32_MAX) {
                printf("func %" PRIu32 " has the most locals (%" PRIu32
                       ") in this module.\n",
                       m->nimportedfuncs + funcidx_with_most_locals, nlocals);
        }
        if (funcidx_with_most_params != UINT32_MAX) {
                printf("func %" PRIu32 " has the most parameters (%" PRIu32
                       ") in this module.\n",
                       m->nimportedfuncs + funcidx_with_most_params, nparams);
        }
        if (funcidx_with_most_results != UINT32_MAX) {
                printf("func %" PRIu32 " has the most results (%" PRIu32
                       ") in this module.\n",
                       m->nimportedfuncs + funcidx_with_most_results,
                       nresults);
        }
#if defined(TOYWASM_ENABLE_WRITER)
        if (funcidx_with_max_code_size != UINT32_MAX) {
                printf("func %" PRIu32 " is largest (%" PRIu32
                       " bytes) in this module.\n",
                       m->nimportedfuncs + funcidx_with_max_code_size,
                       max_code_size);
        }
#endif

        exit(0);
}
