#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>

#include <toywasm/context.h>
#include <toywasm/type.h>
#if defined(TOYWASM_ENABLE_DYLD)
#include <toywasm/dylink_type.h>
#endif

#include "cstruct.h"

#define ERRCHK(call)                                                          \
        do {                                                                  \
                int ret1 = call;                                              \
                if (ret1 < 0) {                                               \
                        ret = errno;                                          \
                        assert(ret > 0);                                      \
                        goto fail;                                            \
                }                                                             \
        } while (0)

#define PRINT(out, ...) ERRCHK(fprintf(out, __VA_ARGS__))
#define PRINT_CELLIDX(out, ...) ERRCHK(print_cellidx(out, __VA_ARGS__))
#define PRINT_LIMITS(out, ...) ERRCHK(print_limits(out, __VA_ARGS__))
#define PRINT_GLOBALTYPE(out, ...) ERRCHK(print_globaltype(out, __VA_ARGS__))
#define PRINT_TAGTYPE(out, ...) ERRCHK(print_tagtype(out, __VA_ARGS__))
#define PRINT_TABLETYPE(out, ...) ERRCHK(print_tabletype(out, __VA_ARGS__))
#define PRINT_MEMTYPE(out, ...) ERRCHK(print_memtype(out, __VA_ARGS__))
#define PRINT_MEMTYPE(out, ...) ERRCHK(print_memtype(out, __VA_ARGS__))
#define PRINT_GLOBAL(out, ...) ERRCHK(print_global(out, __VA_ARGS__))
#define PRINT_EXPRS(out, ...) ERRCHK(print_exprs(out, __VA_ARGS__))
#define PRINT_EXPRS_LIST(out, ...) ERRCHK(print_exprs_list(out, __VA_ARGS__))
#define PRINT_RESULTTYPE(out, ...) ERRCHK(print_resulttype(out, __VA_ARGS__))
#define PRINT_LOCALTYPE(out, ...) ERRCHK(print_localtype(out, __VA_ARGS__))
#define PRINT_NAME(out, ...) ERRCHK(print_name(out, __VA_ARGS__))
#define PRINT_DYLINK(out, ...) ERRCHK(print_dylink(out, __VA_ARGS__))
#define PRINT_U8_ARRAY_INIT(out, ...)                                         \
        ERRCHK(print_u8_array_init(out, __VA_ARGS__))
#define PRINT_U8_ARRAY_LITERAL(out, ...)                                      \
        ERRCHK(print_u8_array_literal(out, __VA_ARGS__))
#define PRINT_U32_ARRAY_INIT(out, ...)                                        \
        ERRCHK(print_u32_array_init(out, __VA_ARGS__))
#define PRINT_U32_ARRAY_LITERAL(out, ...)                                     \
        ERRCHK(print_u32_array_literal(out, __VA_ARGS__))
#define PRINT_U8_FIELD(out, s, f) PRINT(out, "." #f " = %" PRIu8 ",\n", (s)->f)
#define PRINT_U32_FIELD(out, s, f)                                            \
        PRINT(out, "." #f " = %" PRIu32 ",\n", (s)->f)
#define PRINT_NAME_FIELD(out, s, f)                                           \
        PRINT(out, "." #f " = ");                                             \
        PRINT_NAME(out, &(s)->f)

struct ctx {
        const uint8_t *func_exprs_start;
        const uint8_t *func_exprs_end;
};

#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX) ||                                \
        defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
static int
print_cellidx(FILE *out, const struct localcellidx *idx, uint32_t size)
{
        int ret = 0;
        PRINT(out, "{\n");
        if (idx->cellidxes == NULL) {
                PRINT(out, ".cellidxes = NULL,\n");
        } else {
                PRINT(out, ".cellidxes = (void *)(const uint16_t []){\n");
                uint32_t i;
                for (i = 0; i < size; i++) {
                        PRINT(out, "0x%" PRIx16 ", ", idx->cellidxes[i]);
                }
                PRINT(out, "},\n");
        }
        PRINT(out, "},\n");
fail:
        return ret;
}
#endif

static int
print_resulttype(FILE *out, const struct resulttype *rt)
{
        int ret = 0;
        uint32_t i;
        PRINT(out, "{\n");
        PRINT(out, ".ntypes = %" PRIu32 ",\n", rt->ntypes);
        if (rt->ntypes == 0) {
                PRINT(out, ".types = NULL,\n");
        } else {
                PRINT(out, ".types = (void *)(const enum valtype []){\n");
                for (i = 0; i < rt->ntypes; i++) {
                        PRINT(out, "0x%02" PRIx32 ",\n",
                              (uint32_t)rt->types[i]);
                }
                PRINT(out, "},\n");
        }

        assert(rt->is_static);
        PRINT(out, ".is_static = true,\n");

#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
        PRINT(out, ".cellidx = \n");
        PRINT_CELLIDX(out, &rt->cellidx, rt->ntypes + 1);
#endif

        PRINT(out, "},\n");
fail:
        return ret;
}

static int
print_localtype(FILE *out, const struct localtype *lt)
{
        int ret = 0;
        uint32_t i;
        PRINT(out, "{\n");
        PRINT(out, ".nlocals = %" PRIu32 ",\n", lt->nlocals);
        PRINT(out, ".nlocalchunks = %" PRIu32 ",\n", lt->nlocalchunks);
        if (lt->nlocalchunks == 0) {
                PRINT(out, ".localchunks = NULL,\n");
        } else {
                PRINT(out, ".localchunks = (void *)(const struct localchunk "
                           "[]){\n");
                for (i = 0; i < lt->nlocalchunks; i++) {
                        const struct localchunk *ch = &lt->localchunks[i];
                        PRINT(out, "{\n");
                        PRINT(out, ".type = 0x%02" PRIx32 ",\n",
                              (uint32_t)ch->type);
                        PRINT(out, ".n = 0x%" PRIx32 ",\n", ch->n);
                        PRINT(out, "},\n");
                }
                PRINT(out, "},\n");
        }

#if defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
        PRINT(out, ".cellidx = \n");
        PRINT_CELLIDX(out, &lt->cellidx, lt->nlocals + 1);
#endif

        PRINT(out, "},\n");
fail:
        return ret;
}

static int
print_exprs(FILE *out, const struct expr *expr, const struct ctx *ctx)
{
        int ret = 0;
        uint32_t i;
        PRINT(out, "{\n");

        const uint8_t *sp = expr->start;
        const uint8_t *ep = expr_end(expr);
        uint32_t expr_size = ep - expr->start;

        if (ctx != NULL && ctx->func_exprs_start <= sp &&
            ep <= ctx->func_exprs_end) {
                size_t idx = sp - ctx->func_exprs_start;
                PRINT(out, ".start = &all_funcs_exprs[%zu],\n", idx);
        } else {
                PRINT(out, ".start = (void *)(const uint8_t []){\n");
                for (i = 0; i < expr_size; i++) {
                        PRINT(out, "0x%02" PRIx32 ",\n",
                              (uint32_t)expr->start[i]);
                }
                PRINT(out, "},\n");
        }

#if defined(TOYWASM_MAINTAIN_EXPR_END)
#error TOYWASM_MAINTAIN_EXPR_END not implented
#endif

        const struct expr_exec_info *ei = &expr->ei;
        PRINT(out, ".ei = {\n");
        PRINT(out, ".njumps = %" PRIu32 ",\n", ei->njumps);
        if (ei->njumps == 0) {
                PRINT(out, ".jumps = NULL,\n");
        } else {
                PRINT(out, ".jumps = (void *)(const struct jump []){\n");
                for (i = 0; i < ei->njumps; i++) {
                        const struct jump *j = &ei->jumps[i];
                        PRINT(out, "{\n");
                        PRINT(out, ".pc = 0x%" PRIx32 ",\n", j->pc);
                        PRINT(out, ".targetpc = 0x%" PRIx32 ",\n",
                              j->targetpc);
                        PRINT(out, "},\n");
                }
                PRINT(out, "},\n");
        }
        PRINT(out, ".maxlabels = %" PRIu32 ",\n", ei->maxlabels);
        PRINT(out, ".maxcells = %" PRIu32 ",\n", ei->maxcells);
#if defined(TOYWASM_USE_SMALL_CELLS)
        const struct type_annotations *an = &ei->type_annotations;
        PRINT(out, ".type_annotations = {\n");
        PRINT(out, ".default_size = %" PRIu32 ",\n", an->default_size);
        PRINT(out, ".ntypes = %" PRIu32 ",\n", an->ntypes);
        if (an->ntypes == 0) {
                PRINT(out, ".types = NULL,\n");
        } else {
                PRINT(out, ".types = (void *)(const struct "
                           "type_annotation[]){\n");
                for (i = 0; i < an->ntypes; i++) {
                        const struct type_annotation *a = &an->types[i];
                        PRINT(out, "{\n");
                        PRINT(out, ".pc = 0x%" PRIx32 ",\n", a->pc);
                        PRINT(out, ".size = 0x%" PRIx32 ",\n", a->size);
                        PRINT(out, "},\n");
                }
                PRINT(out, "},\n");
        }
        PRINT(out, "},\n");
#endif
        PRINT(out, "},\n");

        PRINT(out, "},\n");
fail:
        return ret;
}

static int
print_limits(FILE *out, const struct limits *lim)
{
        int ret = 0;
        PRINT(out, "{\n");
        PRINT(out, ".min = 0x%" PRIx32 ",\n", lim->min);
        PRINT(out, ".max = 0x%" PRIx32 ",\n", lim->max);
        PRINT(out, "},\n");
fail:
        return ret;
}

static int
print_tabletype(FILE *out, const struct tabletype *type)
{
        int ret = 0;
        PRINT(out, "{\n");
        PRINT(out, ".et = 0x%02" PRIx32 ",\n", (uint32_t)type->et);
        PRINT(out, ".lim = ");
        PRINT_LIMITS(out, &type->lim);
        PRINT(out, "},\n");
fail:
        return ret;
}

static int
print_memtype(FILE *out, const struct memtype *type)
{
        int ret = 0;
        PRINT(out, "{\n");
        PRINT(out, ".lim = ");
        PRINT_LIMITS(out, &type->lim);
        PRINT(out, ".flags = 0x%02" PRIx8 ",\n", type->flags);
#if defined(TOYWASM_ENABLE_WASM_CUSTOM_PAGE_SIZES)
        PRINT_U8_FIELD(out, type, page_shift);
#endif
        PRINT(out, "},\n");
fail:
        return ret;
}

static int
print_globaltype(FILE *out, const struct globaltype *type)
{
        int ret = 0;
        PRINT(out, "{\n");
        PRINT(out, ".t = 0x%02" PRIx32 ",\n", (uint32_t)type->t);
        PRINT(out, ".mut = 0x%02" PRIx32 ",\n", (uint32_t)type->mut);
        PRINT(out, "},\n");
fail:
        return ret;
}

#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
static int
print_tagtype(FILE *out, const struct tagtype *type)
{
        int ret = 0;
        PRINT(out, "{\n");
        PRINT_U32_FIELD(out, type, typeidx);
        PRINT(out, "},\n");
fail:
        return ret;
}
#endif

static int
print_global(FILE *out, const struct global *g, const struct ctx *ctx)
{
        int ret = 0;
        PRINT(out, "{\n");
        PRINT(out, ".type = ");
        PRINT_GLOBALTYPE(out, &g->type);
        PRINT(out, ".init = ");
        PRINT_EXPRS(out, &g->init, ctx);
        PRINT(out, "},\n");
fail:
        return ret;
}

static int
print_u8_array_init(FILE *out, const uint8_t *p, uint32_t size)
{
        int ret = 0;
        uint32_t i;
        PRINT(out, "{\n");
        for (i = 0; i < size; i++) {
                PRINT(out, "0x%02" PRIx32 ",\n", (uint32_t)p[i]);
        }
        PRINT(out, "}");
fail:
        return ret;
}

static int
print_u8_array_literal(FILE *out, const uint8_t *p, uint32_t size)
{
        int ret = 0;
        PRINT(out, "(const uint8_t [])\n");
        PRINT_U8_ARRAY_INIT(out, p, size);
        PRINT(out, ",\n");
fail:
        return ret;
}

static int
print_u32_array_init(FILE *out, const uint32_t *p, uint32_t size)
{
        int ret = 0;
        uint32_t i;
        PRINT(out, "{\n");
        for (i = 0; i < size; i++) {
                PRINT(out, "0x%" PRIx32 ",\n", p[i]);
        }
        PRINT(out, "}");
fail:
        return ret;
}

static int
print_u32_array_literal(FILE *out, const uint32_t *p, uint32_t size)
{
        int ret = 0;
        PRINT(out, "(const uint32_t [])\n");
        PRINT_U32_ARRAY_INIT(out, p, size);
        PRINT(out, ",\n");
fail:
        return ret;
}

static int
print_exprs_list(FILE *out, const struct expr *p, uint32_t size,
                 const struct ctx *ctx)
{
        int ret = 0;
        uint32_t i;
        PRINT(out, "(const struct expr []){\n");
        for (i = 0; i < size; i++) {
                PRINT_EXPRS(out, &p[i], ctx);
        }
        PRINT(out, "},\n");
fail:
        return ret;
}

static int
print_name(FILE *out, const struct name *name)
{
        int ret = 0;
        PRINT(out, "{\n");
        PRINT(out, ".nbytes = %" PRIu32 ",\n", name->nbytes);
        if (name->nbytes == 0) {
                PRINT(out, ".data = NULL,\n");
        } else {
                PRINT(out, ".data = (void *)");
                PRINT_U8_ARRAY_LITERAL(out, (const uint8_t *)name->data,
                                       name->nbytes);
        }
        PRINT(out, "},\n");
fail:
        return ret;
}

#if defined(TOYWASM_ENABLE_DYLD)
static int
print_dylink(FILE *out, const struct dylink *dylink)
{
        int ret = 0;
        uint32_t i;
        PRINT(out, "{\n");

        PRINT(out, ".mem_info = {\n");
        PRINT_U32_FIELD(out, &(dylink->mem_info), memorysize);
        PRINT_U32_FIELD(out, &(dylink->mem_info), memoryalignment);
        PRINT_U32_FIELD(out, &(dylink->mem_info), tablesize);
        PRINT_U32_FIELD(out, &(dylink->mem_info), tablealignment);
        PRINT(out, "},\n");

        PRINT(out, ".needs = {\n");
        PRINT_U32_FIELD(out, &(dylink->needs), count);
        PRINT(out, ".names = (void *)(const struct name[]){\n");
        for (i = 0; i < dylink->needs.count; i++) {
                PRINT_NAME(out, &dylink->needs.names[i]);
        }
        PRINT(out, "},\n");
        PRINT(out, "},\n");

        PRINT_U32_FIELD(out, dylink, nimport_info);
        PRINT(out,
              ".import_info = (void *)(const struct dylink_import_info[]){\n");
        for (i = 0; i < dylink->nimport_info; i++) {
                const struct dylink_import_info *ii = &dylink->import_info[i];
                PRINT(out, "{\n");
                PRINT_NAME_FIELD(out, ii, module_name);
                PRINT_NAME_FIELD(out, ii, name);
                PRINT_U32_FIELD(out, ii, flags);
                PRINT(out, "},\n");
        }
        PRINT(out, "},\n");

        PRINT(out, "},\n");
fail:
        return ret;
}
#endif

int
dump_module_as_cstruct(FILE *out, const char *name, const struct module *m)
{
        struct ctx ctx;
        uint32_t pc_offset = 0;
        uint32_t i;
        int ret = 0;

        PRINT(out, "/* generated by toywasm wasm2cstruct */\n");

        PRINT(out, "#include <stddef.h>\n");

        PRINT(out, "#include <toywasm/type.h>\n");
#if defined(TOYWASM_ENABLE_DYLD)
        PRINT(out, "#include <toywasm/dylink_type.h>\n");
#endif
        PRINT(out, "#include <toywasm/util.h>\n");

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
                PRINT(out, "static const uint8_t all_funcs_exprs[] = {\n");
                for (i = 0; i < size; i++) {
                        PRINT(out, "0x%" PRIx8 ",", sp[i]);
                }
                PRINT(out, "};\n");
                ctx.func_exprs_start = sp;
                ctx.func_exprs_end = ep;
                pc_offset = ptr2pc(m, sp);
        }

        PRINT(out, "static const struct functype types[] = {\n");
        for (i = 0; i < m->ntypes; i++) {
                const struct functype *ft = &m->types[i];
                PRINT(out, "{\n");
                PRINT(out, ".parameter = ");
                PRINT_RESULTTYPE(out, &ft->parameter);
                PRINT(out, ".result = ");
                PRINT_RESULTTYPE(out, &ft->result);
                PRINT(out, "},\n");
        }
        PRINT(out, "};\n");

        PRINT(out, "static const struct func funcs[] = {\n");
        for (i = 0; i < m->nfuncs; i++) {
                const struct func *func = &m->funcs[i];
                PRINT(out, "{\n");
                PRINT(out, ".localtype = ");
                PRINT_LOCALTYPE(out, &func->localtype);
                PRINT(out, ".e = ");
                PRINT_EXPRS(out, &func->e, &ctx);
                PRINT(out, "},\n");
        }
        PRINT(out, "};\n");

        PRINT(out, "static const uint32_t functypeidxes[] = {\n");
        for (i = 0; i < m->nfuncs; i++) {
                PRINT(out, "%" PRIu32 ",", m->functypeidxes[i]);
        }
        PRINT(out, "};\n");

        PRINT(out, "static const struct tabletype tables[] = {\n");
        for (i = 0; i < m->ntables; i++) {
                PRINT_TABLETYPE(out, &m->tables[i]);
        }
        PRINT(out, "};\n");

        PRINT(out, "static const struct memtype mems[] = {\n");
        for (i = 0; i < m->nmems; i++) {
                PRINT_MEMTYPE(out, &m->mems[i]);
        }
        PRINT(out, "};\n");

        PRINT(out, "static const struct global globals[] = {\n");
        for (i = 0; i < m->nglobals; i++) {
                PRINT_GLOBAL(out, &m->globals[i], &ctx);
        }
        PRINT(out, "};\n");

#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        PRINT(out, "static const struct tagtype tags[] = {\n");
        for (i = 0; i < m->ntags; i++) {
                PRINT_TAGTYPE(out, &m->tags[i]);
        }
        PRINT(out, "};\n");
#endif

        PRINT(out, "static const struct element elems[] = {\n");
        for (i = 0; i < m->nelems; i++) {
                const struct element *e = &m->elems[i];
                PRINT(out, "{\n");
                if (e->funcs != NULL) {
                        PRINT(out, ".funcs = (void *)");
                        PRINT_U32_ARRAY_LITERAL(out, e->funcs, e->init_size);
                } else {
                        PRINT(out, ".init_exprs = (void *)");
                        PRINT_EXPRS_LIST(out, e->init_exprs, e->init_size,
                                         &ctx);
                }
                PRINT(out, ".init_size = %" PRIu32 ",\n", e->init_size);
                PRINT(out, ".type = 0x%02" PRIx32 ",\n", (uint32_t)e->type);
                PRINT(out, ".mode = 0x%02" PRIx32 ",\n", (uint32_t)e->mode);
                PRINT(out, ".table = %" PRIu32 ",\n", e->table);
                PRINT(out, ".offset = ");
                PRINT_EXPRS(out, &e->offset, &ctx);
                PRINT(out, "},\n");
        }
        PRINT(out, "};\n");

        PRINT(out, "static const struct data datas[] = {\n");
        for (i = 0; i < m->ndatas; i++) {
                const struct data *d = &m->datas[i];
                PRINT(out, "{\n");
                PRINT(out, ".mode = %" PRIu32 ",\n", (uint32_t)d->mode);
                PRINT(out, ".init_size = %" PRIu32 ",\n", d->init_size);
                PRINT(out, ".memory = %" PRIu32 ",\n", d->memory);
                PRINT(out, ".offset = ");
                PRINT_EXPRS(out, &d->offset, &ctx);
                PRINT(out, ".init = (void *)");
                PRINT_U8_ARRAY_LITERAL(out, d->init, d->init_size);
                PRINT(out, "},\n");
        }
        PRINT(out, "};\n");

        PRINT(out, "static const struct import imports[] = {\n");
        for (i = 0; i < m->nimports; i++) {
                const struct import *im = &m->imports[i];
                PRINT(out, "{\n");
                PRINT(out, ".module_name = ");
                PRINT_NAME(out, &im->module_name);
                PRINT(out, ".name = ");
                PRINT_NAME(out, &im->name);
                const struct importdesc *desc = &im->desc;
                PRINT(out, ".desc = {");
                PRINT(out, ".type = %02" PRIu32 ",\n", (uint32_t)desc->type);
                PRINT(out, ".u = {");
                switch (desc->type) {
                case EXTERNTYPE_FUNC:
                        PRINT(out, ".typeidx = %" PRIu32 ",\n",
                              desc->u.typeidx);
                        break;
                case EXTERNTYPE_TABLE:
                        PRINT(out, ".tabletype = ");
                        PRINT_TABLETYPE(out, &desc->u.tabletype);
                        break;
                case EXTERNTYPE_MEMORY:
                        PRINT(out, ".memtype = ");
                        PRINT_MEMTYPE(out, &desc->u.memtype);
                        break;
                case EXTERNTYPE_GLOBAL:
                        PRINT(out, ".globaltype = ");
                        PRINT_GLOBALTYPE(out, &desc->u.globaltype);
                        break;
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
                case EXTERNTYPE_TAG:
                        PRINT(out, ".tagtype = ");
                        PRINT_TAGTYPE(out, &desc->u.tagtype);
                        break;
#endif
                }
                PRINT(out, "},\n");
                PRINT(out, "},\n");
                PRINT(out, "},\n");
        }
        PRINT(out, "};\n");

        PRINT(out, "static const struct wasm_export exports[] = {\n");
        for (i = 0; i < m->nexports; i++) {
                const struct wasm_export *ex = &m->exports[i];
                const struct exportdesc *desc = &ex->desc;
                PRINT(out, "{\n");
                PRINT(out, ".name = ");
                PRINT_NAME(out, &ex->name);
                PRINT(out, ".desc = {\n");
                PRINT(out, ".type = %" PRIu32 ",\n", desc->type);
                PRINT(out, ".idx = %" PRIu32 ",\n", desc->idx);
                PRINT(out, "},\n");
                PRINT(out, "},\n");
        }
        PRINT(out, "};\n");

#if defined(TOYWASM_ENABLE_WASM_NAME_SECTION)
        if (m->name_section_start != NULL) {
                PRINT(out, "static const uint8_t name_section[] = \n");
                size_t size = m->name_section_end - m->name_section_start;
                PRINT_U8_ARRAY_INIT(out, m->name_section_start, size);
                PRINT(out, ";\n");
        }
#endif

        PRINT(out, "const struct module %s = {\n", name);

        PRINT(out, "    .ntypes = ARRAYCOUNT(types),\n");
        PRINT(out, "    .types = (void *)types,\n");

        PRINT(out, "    .nimportedfuncs = %" PRIu32 ",\n", m->nimportedfuncs);
        PRINT(out, "    .nfuncs = ARRAYCOUNT(funcs),\n");
        PRINT(out, "    .functypeidxes = (void *)functypeidxes,\n");
        PRINT(out, "    .funcs = (void *)funcs,\n");

        PRINT(out, "    .nimportedtables = %" PRIu32 ",\n",
              m->nimportedtables);
        PRINT(out, "    .ntables = ARRAYCOUNT(tables),\n");
        PRINT(out, "    .tables = (void *)tables,\n");

        PRINT(out, "    .nimportedmems = %" PRIu32 ",\n", m->nimportedmems);
        PRINT(out, "    .nmems = ARRAYCOUNT(mems),\n");
        PRINT(out, "    .mems = (void *)mems,\n");

        PRINT(out, "    .nimportedglobals = %" PRIu32 ",\n",
              m->nimportedglobals);
        PRINT(out, "    .nglobals = ARRAYCOUNT(globals),\n");
        PRINT(out, "    .globals = (void *)globals,\n");

#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        PRINT_U32_FIELD(out, m, nimportedtags);
        PRINT(out, "    .ntags = ARRAYCOUNT(tags),\n");
        PRINT(out, "    .tags = (void *)tags,\n");
#endif

        PRINT(out, "    .nelems = ARRAYCOUNT(elems),\n");
        PRINT(out, "    .elems = (void *)elems,\n");

        PRINT(out, "    .ndatas = ARRAYCOUNT(datas),\n");
        PRINT(out, "    .datas = (void *)datas,\n");

        PRINT(out, "    .has_start = %s,\n", m->has_start ? "true" : "false");
        PRINT(out, "    .start = %" PRIu32 ",\n", m->start);

        PRINT(out, "    .nimports = ARRAYCOUNT(imports),\n");
        PRINT(out, "    .imports = (void *)imports,\n");

        PRINT(out, "    .nexports = ARRAYCOUNT(exports),\n");
        PRINT(out, "    .exports = (void *)exports,\n");

        if (m->nfuncs > 0) {
                PRINT(out, "    .bin = all_funcs_exprs - %" PRIu32 ",\n",
                      pc_offset);
        }

#if defined(TOYWASM_ENABLE_WASM_NAME_SECTION)
        if (m->name_section_start != NULL) {
                PRINT(out, ".name_section_start = name_section,\n");
                PRINT(out, ".name_section_end = name_section + %zu,\n",
                      m->name_section_end - m->name_section_start);
        }
#endif
#if defined(TOYWASM_ENABLE_DYLD)
        if (m->dylink != NULL) {
                PRINT(out, ".dylink = (void *)&(const struct dylink)");
                PRINT_DYLINK(out, m->dylink);
        }
#endif

        PRINT(out, "};\n");
fail:
        return ret;
}
