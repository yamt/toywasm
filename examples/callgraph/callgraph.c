#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <toywasm/context.h>
#include <toywasm/expr_parser.h>
#include <toywasm/leb128.h>
#include <toywasm/name.h>
#include <toywasm/type.h>

#include "jsonutil.h"

static void
fatal(int error)
{
        fprintf(stderr, "fatal error %d: %s", error, strerror(error));
        exit(1);
}

static json_t *
dump_calls(const struct module *m, uint32_t i, struct nametable *table)
{
        const struct func *func = &m->funcs[i];
        const uint8_t *insn = func->e.start;
        json_t *a = json_array();
        if (a == NULL) {
                json_fatal();
        }
        struct parse_expr_context ctx;
        parse_expr_context_init(&ctx);
        do {
                const uint8_t *imm;
                uint32_t callee;
                uint32_t tableidx;
                uint32_t typeidx;
                const struct functype *ft;
                char *typestr;
                int ret;
                uint32_t pc = ptr2pc(m, insn);
                switch (insn[0]) {
                case 0x10: /* call */
                        imm = &insn[1];
                        callee = read_leb_u32_nocheck(&imm);
                        json_pack_and_append(a, "{sisi}", "pc", pc, "callee",
                                             callee);
                        break;
                case 0x11: /* call_indirect */
                        imm = &insn[1];
                        typeidx = read_leb_u32_nocheck(&imm);
                        tableidx = read_leb_u32_nocheck(&imm);
                        ft = &m->types[typeidx];
                        ret = functype_to_string(&typestr, ft);
                        if (ret != 0) {
                                fatal(ret);
                        }
                        json_pack_and_append(a, "{sisiss}", "pc", pc, "table",
                                             tableidx, "type", typestr);
                        functype_string_free(typestr);
                        break;
#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
                        /* XXX implement tail-call instructions */
#endif
                default:
                        break;
                }
                parse_expr(&insn, &ctx);
        } while (insn != NULL);
        parse_expr_context_clear(&ctx);
        return a;
}

/*
 * print the function callgraph of the given module in json.
 */
void
callgraph(const struct module *m)
{
        int ret;
        uint32_t i;
        json_t *j = json_object();
        if (j == NULL) {
                json_fatal();
        }
        json_t *a;
        struct nametable table;
        nametable_init(&table);
        a = json_object_set_array(j, "funcs");
        for (i = 0; i < m->nimportedfuncs + m->nfuncs; i++) {
                bool imported = i < m->nimportedfuncs;
                struct name func_name;
                nametable_lookup_func(&table, m, i, &func_name);
                const struct functype *ft = module_functype(m, i);
                char *typestr;
                ret = functype_to_string(&typestr, ft);
                if (ret != 0) {
                        fatal(ret);
                }
                json_t *calls = NULL;
                if (!imported) {
                        calls = dump_calls(m, i - m->nimportedfuncs, &table);
                }
                json_pack_and_append(a, "{ss#sisssbso*}", "name",
                                     func_name.data, (int)func_name.nbytes,
                                     "idx", i, "type", typestr, "imported",
                                     (int)imported, "calls", calls);
                functype_string_free(typestr);
        }
        /*
         * here we only implement active elements with funcidxes.
         * ie. the case where we don't need to execute exprs.
         * although it's limited, it should be enough to
         * cover the most of common cases.
         */
        a = json_object_set_array(j, "elements");
        for (i = 0; i < m->nelems; i++) {
                const struct element *e = &m->elems[i];
                if (e->mode != ELEM_MODE_ACTIVE) {
                        continue;
                }
                if (e->funcs == NULL) {
                        continue;
                }
                uint32_t j;
                for (j = 0; j < e->init_size; j++) {
                        uint32_t funcidx = e->funcs[j];
                        uint32_t tableidx = e->table;
                        json_pack_and_append(a, "{sisi}", "tableidx", tableidx,
                                             "funcidx", funcidx);
                }
        }
        a = json_object_set_array(j, "imports");
        for (i = 0; i < m->nimports; i++) {
                const struct import *im = &m->imports[i];
                if (im->desc.type != EXTERNTYPE_FUNC) {
                        continue;
                }
                json_pack_and_append(
                        a, "{ss#ss#si}", "module_name", im->module_name.data,
                        (int)im->module_name.nbytes, "name", im->name.data,
                        (int)im->name.nbytes, "idx", i);
        }
        a = json_object_set_array(j, "exports");
        for (i = 0; i < m->nexports; i++) {
                const struct wasm_export *ex = &m->exports[i];
                if (ex->desc.type != EXTERNTYPE_FUNC) {
                        continue;
                }
                json_pack_and_append(a, "{ss#si}", "name", ex->name.data,
                                     (int)ex->name.nbytes, "idx",
                                     ex->desc.idx);
        }
        if (m->has_start) {
                json_object_set_u32(j, "start", m->start);
        }
        nametable_clear(&table);
        json_dumpf(j, stdout, JSON_INDENT(4));
        printf("\n");
}
