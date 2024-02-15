#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <toywasm/fileio.h>
#include <toywasm/load_context.h>
#include <toywasm/module.h>
#include <toywasm/name.h>
#include <toywasm/type.h>
#include <toywasm/xlog.h>

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
                xlog_error("module_load failed with %d", ret);
                exit(1);
        }
        module_print_stats(m);

        /* perform some investigations on the loaded module */
        printf("module imports %" PRIu32 " functions\n", m->nimportedfuncs);
        printf("module contains %" PRIu32 " functions\n", m->nfuncs);
        uint32_t i;
        uint32_t funcidx_with_most_locals = UINT32_MAX;
        uint32_t nlocals = 0;
        uint32_t funcidx_with_most_params = UINT32_MAX;
        uint32_t nparams = 0;
        uint32_t funcidx_with_most_results = UINT32_MAX;
        uint32_t nresults = 0;
        uint32_t funcidx_with_largest_jump_table = UINT32_MAX;
        uint32_t largest_jump_table = 0;
#if defined(TOYWASM_USE_SMALL_CELLS)
        uint32_t funcidx_with_largest_type_annotations = UINT32_MAX;
        uint32_t largest_type_annotations = 0;
#endif
#if defined(TOYWASM_ENABLE_WRITER)
        uint32_t funcidx_with_max_code_size = UINT32_MAX;
        uint32_t max_code_size = 0;
#endif
        for (i = 0; i < m->nfuncs; i++) {
                const struct func *func = &m->funcs[i];
                const struct localtype *lt = &func->localtype;
                const struct expr_exec_info *ei = &func->e.ei;
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
                if (funcidx_with_largest_jump_table == UINT32_MAX ||
                    ei->njumps > largest_jump_table) {
                        funcidx_with_largest_jump_table = i;
                        largest_jump_table = ei->njumps;
                }
#if defined(TOYWASM_USE_SMALL_CELLS)
                if (funcidx_with_largest_type_annotations == UINT32_MAX ||
                    ei->type_annotations.ntypes > largest_type_annotations) {
                        funcidx_with_largest_type_annotations = i;
                        largest_type_annotations = ei->type_annotations.ntypes;
                }
#endif
#if defined(TOYWASM_ENABLE_WRITER)
                uint32_t code_size = func->e.end - func->e.start;
                if (funcidx_with_max_code_size == UINT32_MAX ||
                    code_size > max_code_size) {
                        funcidx_with_max_code_size = i;
                        max_code_size = code_size;
                }
#endif
        }

        struct nametable table;
        nametable_init(&table);
        if (funcidx_with_most_locals != UINT32_MAX) {
                uint32_t funcidx =
                        m->nimportedfuncs + funcidx_with_most_locals;
                struct name func_name;
                nametable_lookup_func(&table, m, funcidx, &func_name);
                printf("func %" PRIu32 " (%.*s) has the most locals (%" PRIu32
                       ") in this module.\n",
                       funcidx, CSTR(&func_name), nlocals);
        }
        if (funcidx_with_most_params != UINT32_MAX) {
                uint32_t funcidx =
                        m->nimportedfuncs + funcidx_with_most_params;
                struct name func_name;
                nametable_lookup_func(&table, m, funcidx, &func_name);
                printf("func %" PRIu32
                       " (%.*s) has the most parameters (%" PRIu32
                       ") in this module.\n",
                       funcidx, CSTR(&func_name), nparams);
        }
        if (funcidx_with_most_results != UINT32_MAX) {
                uint32_t funcidx =
                        m->nimportedfuncs + funcidx_with_most_params;
                struct name func_name;
                nametable_lookup_func(&table, m, funcidx, &func_name);
                printf("func %" PRIu32 " (%.*s) has the most results (%" PRIu32
                       ") in this module.\n",
                       funcidx, CSTR(&func_name), nresults);
        }
        if (funcidx_with_largest_jump_table != UINT32_MAX) {
                uint32_t funcidx =
                        m->nimportedfuncs + funcidx_with_largest_jump_table;
                struct name func_name;
                nametable_lookup_func(&table, m, funcidx, &func_name);
                printf("func %" PRIu32
                       " (%.*s) has the largest jump table (%" PRIu32
                       " entries) in this module.\n",
                       funcidx, CSTR(&func_name), largest_jump_table);
        }
#if defined(TOYWASM_USE_SMALL_CELLS)
        if (funcidx_with_largest_type_annotations != UINT32_MAX) {
                uint32_t funcidx = m->nimportedfuncs +
                                   funcidx_with_largest_type_annotations;
                struct name func_name;
                nametable_lookup_func(&table, m, funcidx, &func_name);
                printf("func %" PRIu32
                       " (%.*s) has the largest type annotations (%" PRIu32
                       " entries) in this module.\n",
                       funcidx, CSTR(&func_name), largest_type_annotations);
        }
#endif
#if defined(TOYWASM_ENABLE_WRITER)
        if (funcidx_with_max_code_size != UINT32_MAX) {
                uint32_t funcidx =
                        m->nimportedfuncs + funcidx_with_max_code_size;
                struct name func_name;
                nametable_lookup_func(&table, m, funcidx, &func_name);
                printf("func %" PRIu32 " (%.*s) is largest (%" PRIu32
                       " bytes) in this module.\n",
                       funcidx, CSTR(&func_name), max_code_size);
        }
#endif
        nametable_clear(&table);

        exit(0);
}
