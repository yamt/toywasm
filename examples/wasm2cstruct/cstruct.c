#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

#include <toywasm/context.h>
#include <toywasm/type.h>

#include "cstruct.h"

struct ctx {
        const uint8_t *func_exprs_start;
        const uint8_t *func_exprs_end;
};

static int
print_cellidx(FILE *out, const struct localcellidx *idx, uint32_t size)
{
        fprintf(out, "{\n");
        if (idx->cellidxes == NULL) {
                fprintf(out, ".cellidxes = NULL,\n");
        } else {
                fprintf(out, ".cellidxes = (void *)(const uint16_t []){\n");
                uint32_t i;
                for (i = 0; i < size; i++) {
                        fprintf(out, "0x%" PRIx16 ", ", idx->cellidxes[i]);
                }
                fprintf(out, "},\n");
        }
        fprintf(out, "},\n");
        return 0;
}

static int
print_resulttype(FILE *out, const struct resulttype *rt)
{
        uint32_t i;
        fprintf(out, "{\n");
        fprintf(out, ".ntypes = %" PRIu32 ",\n", rt->ntypes);
        if (rt->ntypes == 0) {
                fprintf(out, ".types = NULL,\n");
        } else {
                fprintf(out, ".types = (void *)(const enum valtype []){\n");
                for (i = 0; i < rt->ntypes; i++) {
                        fprintf(out, "0x%02" PRIx32 ",\n",
                                (uint32_t)rt->types[i]);
                }
                fprintf(out, "},\n");
        }

        assert(rt->is_static);
        fprintf(out, ".is_static = true,\n");

#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
        fprintf(out, ".cellidx = \n");
        print_cellidx(out, &rt->cellidx, rt->ntypes + 1);
#endif

        fprintf(out, "},\n");
        return 0;
}

static int
print_localtype(FILE *out, const struct localtype *lt)
{
        uint32_t i;
        fprintf(out, "{\n");
        fprintf(out, ".nlocals = %" PRIu32 ",\n", lt->nlocals);
        fprintf(out, ".nlocalchunks = %" PRIu32 ",\n", lt->nlocalchunks);
        if (lt->nlocalchunks == 0) {
                fprintf(out, ".localchunks = NULL,\n");
        } else {
                fprintf(out, ".localchunks = (void *)(const struct localchunk "
                             "[]){\n");
                for (i = 0; i < lt->nlocalchunks; i++) {
                        const struct localchunk *ch = &lt->localchunks[i];
                        fprintf(out, "{\n");
                        fprintf(out, ".type = 0x%02" PRIx32 ",\n",
                                (uint32_t)ch->type);
                        fprintf(out, ".n = 0x%" PRIx32 ",\n", ch->n);
                        fprintf(out, "},\n");
                }
                fprintf(out, "},\n");
        }

#if defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
        fprintf(out, ".cellidx = \n");
        print_cellidx(out, &lt->cellidx, lt->nlocals + 1);
#endif

        fprintf(out, "},\n");
        return 0;
}

static int
print_exprs(FILE *out, const struct expr *expr, const struct ctx *ctx)
{
        uint32_t i;
        fprintf(out, "{\n");

        const uint8_t *sp = expr->start;
        const uint8_t *ep = expr_end(expr);
        uint32_t expr_size = ep - expr->start;

        if (ctx != NULL && ctx->func_exprs_start <= sp &&
            ep <= ctx->func_exprs_end) {
                size_t idx = sp - ctx->func_exprs_start;
                fprintf(out, ".start = &all_funcs_exprs[%zu],\n", idx);
        } else {
                fprintf(out, ".start = (void *)(const uint8_t []){\n");
                for (i = 0; i < expr_size; i++) {
                        fprintf(out, "0x%02" PRIx32 ",\n",
                                (uint32_t)expr->start[i]);
                }
                fprintf(out, "},\n");
        }

        /* TODO: TOYWASM_MAINTAIN_EXPR_END */

        const struct expr_exec_info *ei = &expr->ei;
        fprintf(out, ".ei = {\n");
        fprintf(out, ".njumps = %" PRIu32 ",\n", ei->njumps);
        if (ei->njumps == 0) {
                fprintf(out, ".jumps = NULL,\n");
        } else {
                fprintf(out, ".jumps = (void *)(const struct jump []){\n");
                for (i = 0; i < ei->njumps; i++) {
                        const struct jump *j = &ei->jumps[i];
                        fprintf(out, "{\n");
                        fprintf(out, ".pc = 0x%" PRIx32 ",\n", j->pc);
                        fprintf(out, ".targetpc = 0x%" PRIx32 ",\n",
                                j->targetpc);
                        fprintf(out, "},\n");
                }
                fprintf(out, "},\n");
        }
        fprintf(out, ".maxlabels = %" PRIu32 ",\n", ei->maxlabels);
        fprintf(out, ".maxcells = %" PRIu32 ",\n", ei->maxcells);
#if defined(TOYWASM_USE_SMALL_CELLS)
        const struct type_annotations *an = &ei->type_annotations;
        fprintf(out, ".type_annotations = {\n");
        fprintf(out, ".default_size = %" PRIu32 ",\n", an->default_size);
        fprintf(out, ".ntypes = %" PRIu32 ",\n", an->ntypes);
        if (an->ntypes == 0) {
                fprintf(out, ".types = NULL,\n");
        } else {
                fprintf(out, ".types = (void *)(const struct "
                             "type_annotation[]){\n");
                for (i = 0; i < an->ntypes; i++) {
                        const struct type_annotation *a = &an->types[i];
                        fprintf(out, "{\n");
                        fprintf(out, ".pc = 0x%" PRIx32 ",\n", a->pc);
                        fprintf(out, ".size = 0x%" PRIx32 ",\n", a->size);
                        fprintf(out, "},\n");
                }
                fprintf(out, "},\n");
        }
        fprintf(out, "},\n");
#endif
        fprintf(out, "},\n");

        fprintf(out, "},\n");
        return 0;
}

static int
print_limits(FILE *out, const struct limits *lim)
{
        fprintf(out, "{\n");
        fprintf(out, ".min = 0x%" PRIx32 ",\n", lim->min);
        fprintf(out, ".max = 0x%" PRIx32 ",\n", lim->max);
        fprintf(out, "},\n");
        return 0;
}

static int
print_tabletype(FILE *out, const struct tabletype *type)
{
        fprintf(out, "{\n");
        fprintf(out, ".et = 0x%02" PRIx32 ",\n", (uint32_t)type->et);
        fprintf(out, ".lim = ");
        print_limits(out, &type->lim);
        fprintf(out, "},\n");
        return 0;
}

static int
print_memtype(FILE *out, const struct memtype *type)
{
        fprintf(out, "{\n");
        fprintf(out, ".lim = ");
        print_limits(out, &type->lim);
        fprintf(out, ".flags = 0x%02" PRIx8 ",\n", type->flags);
        /* TODO: TOYWASM_ENABLE_WASM_CUSTOM_PAGE_SIZES */
        fprintf(out, "},\n");
        return 0;
}

static int
print_globaltype(FILE *out, const struct globaltype *type)
{
        fprintf(out, "{\n");
        fprintf(out, ".t = 0x%02" PRIx32 ",\n", (uint32_t)type->t);
        fprintf(out, ".mut = 0x%02" PRIx32 ",\n", (uint32_t)type->mut);
        fprintf(out, "},\n");
        return 0;
}

static int
print_global(FILE *out, const struct global *g, const struct ctx *ctx)
{
        fprintf(out, "{\n");
        fprintf(out, ".type = ");
        print_globaltype(out, &g->type);
        fprintf(out, ".init = ");
        print_exprs(out, &g->init, ctx);
        fprintf(out, "},\n");
        return 0;
}

static int
print_bytes(FILE *out, const uint8_t *p, uint32_t size)
{
        uint32_t i;
        fprintf(out, "(const uint8_t []){\n");
        for (i = 0; i < size; i++) {
                fprintf(out, "0x%02" PRIx32 ",\n", (uint32_t)p[i]);
        }
        fprintf(out, "},\n");
        return 0;
}

static int
print_u32_list(FILE *out, const uint32_t *p, uint32_t size)
{
        uint32_t i;
        fprintf(out, "(const uint32_t []){\n");
        for (i = 0; i < size; i++) {
                fprintf(out, "0x%" PRIx32 ",", p[i]);
        }
        fprintf(out, "},\n");
        return 0;
}

static int
print_exprs_list(FILE *out, const struct expr *p, uint32_t size,
                 const struct ctx *ctx)
{
        uint32_t i;
        fprintf(out, "(const struct expr []){\n");
        for (i = 0; i < size; i++) {
                print_exprs(out, &p[i], ctx);
        }
        fprintf(out, "},\n");
        return 0;
}

static int
print_name(FILE *out, const struct name *name)
{
        fprintf(out, "{\n");
        fprintf(out, ".nbytes = %" PRIu32 ",\n", name->nbytes);
        if (name->nbytes == 0) {
                fprintf(out, ".data = NULL,\n");
        } else {
                fprintf(out, ".data = (void *)");
                print_bytes(out, (const uint8_t *)name->data, name->nbytes);
        }
        fprintf(out, "},\n");
        return 0;
}

int
dump_module_as_cstruct(FILE *out, const char *name, const struct module *m)
{
        struct ctx ctx;
        uint32_t pc_offset = 0;
        uint32_t i;

        fprintf(out, "/* generated by toywasm wasm2cstruct */\n");

        fprintf(out, "#include <stddef.h>\n");

        fprintf(out, "#include <toywasm/type.h>\n");
        fprintf(out, "#include <toywasm/util.h>\n");

        /*
         * Note: toywasm has an assumption any instructions
         * in a module can be addressed by uint32_t "pc".
         * ensure dense placement by using a single array.
         *
         * Note: this is a bit redundant because it includes
         * local definitions. while it's possible to skip them,
         * it's a bit combersome as it would involves "pc" adjustments
         * in annotation tables.
         */
        if (m->nfuncs > 0) {
                const uint8_t *sp = m->funcs[0].e.start;
                const uint8_t *ep = expr_end(&m->funcs[m->nfuncs - 1].e);
                uint32_t size = ep - sp;
                fprintf(out, "static const uint8_t all_funcs_exprs[] = {\n");
                for (i = 0; i < size; i++) {
                        fprintf(out, "0x%" PRIx8 ",", sp[i]);
                }
                fprintf(out, "};\n");
                ctx.func_exprs_start = sp;
                ctx.func_exprs_end = ep;
                pc_offset = ptr2pc(m, sp);
        }

        fprintf(out, "static const struct functype types[] = {\n");
        for (i = 0; i < m->ntypes; i++) {
                const struct functype *ft = &m->types[i];
                fprintf(out, "{\n");
                fprintf(out, ".parameter = ");
                print_resulttype(out, &ft->parameter);
                fprintf(out, ".result = ");
                print_resulttype(out, &ft->result);
                fprintf(out, "},\n");
        }
        fprintf(out, "};\n");

        fprintf(out, "static const struct func funcs[] = {\n");
        for (i = 0; i < m->nfuncs; i++) {
                const struct func *func = &m->funcs[i];
                fprintf(out, "{\n");
                fprintf(out, ".localtype = ");
                print_localtype(out, &func->localtype);
                fprintf(out, ".e = ");
                print_exprs(out, &func->e, &ctx);
                fprintf(out, "},\n");
        }
        fprintf(out, "};\n");

        fprintf(out, "static const uint32_t functypeidxes[] = {\n");
        for (i = 0; i < m->nfuncs; i++) {
                fprintf(out, "%" PRIu32 ",", m->functypeidxes[i]);
        }
        fprintf(out, "};\n");

        fprintf(out, "static const struct tabletype tables[] = {\n");
        for (i = 0; i < m->ntables; i++) {
                print_tabletype(out, &m->tables[i]);
        }
        fprintf(out, "};\n");

        fprintf(out, "static const struct memtype mems[] = {\n");
        for (i = 0; i < m->nmems; i++) {
                print_memtype(out, &m->mems[i]);
        }
        fprintf(out, "};\n");

        fprintf(out, "static const struct global globals[] = {\n");
        for (i = 0; i < m->nglobals; i++) {
                print_global(out, &m->globals[i], &ctx);
        }
        fprintf(out, "};\n");

        fprintf(out, "static const struct element elems[] = {\n");
        for (i = 0; i < m->nelems; i++) {
                const struct element *e = &m->elems[i];
                fprintf(out, "{\n");
                if (e->funcs != NULL) {
                        fprintf(out, ".funcs = (void *)");
                        print_u32_list(out, e->funcs, e->init_size);
                } else {
                        fprintf(out, ".init_exprs = (void *)");
                        print_exprs_list(out, e->init_exprs, e->init_size,
                                         &ctx);
                }
                fprintf(out, ".init_size = %" PRIu32 ",\n", e->init_size);
                fprintf(out, ".type = 0x%02" PRIx32 ",\n", (uint32_t)e->type);
                fprintf(out, ".mode = 0x%02" PRIx32 ",\n", (uint32_t)e->mode);
                fprintf(out, ".table = %" PRIu32 ",\n", e->table);
                fprintf(out, ".offset = ");
                print_exprs(out, &e->offset, &ctx);
                fprintf(out, "},\n");
        }
        fprintf(out, "};\n");

        fprintf(out, "static const struct data datas[] = {\n");
        for (i = 0; i < m->ndatas; i++) {
                const struct data *d = &m->datas[i];
                fprintf(out, "{\n");
                fprintf(out, ".mode = %" PRIu32 ",\n", (uint32_t)d->mode);
                fprintf(out, ".init_size = %" PRIu32 ",\n", d->init_size);
                fprintf(out, ".memory = %" PRIu32 ",\n", d->memory);
                fprintf(out, ".offset = ");
                print_exprs(out, &d->offset, &ctx);
                fprintf(out, ".init = (void *)");
                print_bytes(out, d->init, d->init_size);
                fprintf(out, "},\n");
        }
        fprintf(out, "};\n");

        fprintf(out, "static const struct import imports[] = {\n");
        for (i = 0; i < m->nimports; i++) {
                const struct import *im = &m->imports[i];
                fprintf(out, "{\n");
                fprintf(out, ".module_name = ");
                print_name(out, &im->module_name);
                fprintf(out, ".name = ");
                print_name(out, &im->name);
                const struct importdesc *desc = &im->desc;
                fprintf(out, ".desc = {");
                fprintf(out, ".type = %02" PRIu32 ",\n", (uint32_t)desc->type);
                fprintf(out, ".u = {");
                switch (desc->type) {
                case EXTERNTYPE_FUNC:
                        fprintf(out, ".typeidx = %" PRIu32 ",\n",
                                desc->u.typeidx);
                        break;
                case EXTERNTYPE_TABLE:
                        fprintf(out, ".tabletype = ");
                        print_tabletype(out, &desc->u.tabletype);
                        break;
                case EXTERNTYPE_MEMORY:
                        fprintf(out, ".memtype = ");
                        print_memtype(out, &desc->u.memtype);
                        break;
                case EXTERNTYPE_GLOBAL:
                        fprintf(out, ".globaltype = ");
                        print_globaltype(out, &desc->u.globaltype);
                        break;
                        /* TODO: TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING */
                }
                fprintf(out, "},\n");
                fprintf(out, "},\n");
                fprintf(out, "},\n");
        }
        fprintf(out, "};\n");

        fprintf(out, "static const struct wasm_export exports[] = {\n");
        for (i = 0; i < m->nexports; i++) {
                const struct wasm_export *ex = &m->exports[i];
                const struct exportdesc *desc = &ex->desc;
                fprintf(out, "{\n");
                fprintf(out, ".name = ");
                print_name(out, &ex->name);
                fprintf(out, ".desc = {\n");
                fprintf(out, ".type = %" PRIu32 ",\n", desc->type);
                fprintf(out, ".idx = %" PRIu32 ",\n", desc->idx);
                fprintf(out, "},\n");
                fprintf(out, "},\n");
        }
        fprintf(out, "};\n");

        fprintf(out, "const struct module %s = {\n", name);

        fprintf(out, "    .ntypes = ARRAYCOUNT(types),\n");
        fprintf(out, "    .types = (void *)types,\n");

        fprintf(out, "    .nimportedfuncs = %" PRIu32 ",\n",
                m->nimportedfuncs);
        fprintf(out, "    .nfuncs = ARRAYCOUNT(funcs),\n");
        fprintf(out, "    .functypeidxes = (void *)functypeidxes,\n");
        fprintf(out, "    .funcs = (void *)funcs,\n");

        fprintf(out, "    .nimportedtables = %" PRIu32 ",\n",
                m->nimportedtables);
        fprintf(out, "    .ntables = ARRAYCOUNT(tables),\n");
        fprintf(out, "    .tables = (void *)tables,\n");

        fprintf(out, "    .nimportedmems = %" PRIu32 ",\n", m->nimportedmems);
        fprintf(out, "    .nmems = ARRAYCOUNT(mems),\n");
        fprintf(out, "    .mems = (void *)mems,\n");

        fprintf(out, "    .nimportedglobals = %" PRIu32 ",\n",
                m->nimportedglobals);
        fprintf(out, "    .nglobals = ARRAYCOUNT(globals),\n");
        fprintf(out, "    .globals = (void *)globals,\n");

        /* TODO: tags */

        fprintf(out, "    .nelems = ARRAYCOUNT(elems),\n");
        fprintf(out, "    .elems = (void *)elems,\n");

        fprintf(out, "    .ndatas = ARRAYCOUNT(datas),\n");
        fprintf(out, "    .datas = (void *)datas,\n");

        fprintf(out, "    .has_start = %s,\n",
                m->has_start ? "true" : "false");
        fprintf(out, "    .start = %" PRIu32 ",\n", m->start);

        fprintf(out, "    .nimports = ARRAYCOUNT(imports),\n");
        fprintf(out, "    .imports = (void *)imports,\n");

        fprintf(out, "    .nexports = ARRAYCOUNT(exports),\n");
        fprintf(out, "    .exports = (void *)exports,\n");

        if (m->nfuncs > 0) {
                fprintf(out, "    .bin = all_funcs_exprs - %" PRIu32 ",\n",
                        pc_offset);
        }

        /* TODO TOYWASM_ENABLE_WASM_NAME_SECTION */
        /* TODO TOYWASM_ENABLE_DYLD */

        fprintf(out, "};\n");
        return 0;
}
