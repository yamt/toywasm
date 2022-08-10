/*
 * https://webassembly.github.io/spec/core/binary/modules.html#binary-module
 * https://webassembly.github.io/spec/core/binary/modules.html#sections
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "decode.h"
#include "expr.h"
#include "leb128.h"
#include "load_context.h"
#include "module.h"
#include "report.h"
#include "type.h"
#include "util.h"
#include "xlog.h"

struct section {
        uint8_t id;
        uint32_t size;
        const uint8_t *data;
};

int
section_load(struct section *s, const uint8_t **pp, const uint8_t *ep)
{
        const uint8_t *p = *pp;
        uint8_t v;
        int ret;

        ret = read_u8(&p, ep, &v);
        if (ret != 0) {
                goto fail;
        }
        s->id = v;

        uint32_t v32;
        ret = read_leb_u32(&p, ep, &v32);
        if (ret != 0) {
                goto fail;
        }
        s->size = v32;

        if (p + s->size > ep) {
                ret = EINVAL;
                goto fail;
        }
        s->data = p;
        p += s->size;
        *pp = p;

        return 0;
fail:
        return ret;
}

const char *
valtype_str(enum valtype vt)
{
        static const char *types[] = {
                [TYPE_i32] = "i32",
                [TYPE_i64] = "i64",
                [TYPE_f32] = "f32",
                [TYPE_f64] = "f64",
                [TYPE_v128] = "v128",
                [TYPE_FUNCREF] = "funcref",
                [TYPE_EXTERNREF] = "externref",
        };
        return types[vt];
};

int
read_valtype(const uint8_t **pp, const uint8_t *ep, enum valtype *vt)
{
        const uint8_t *p = *pp;
        uint8_t u8;
        int ret;

        ret = read_u8(&p, ep, &u8);
        if (ret != 0) {
                goto fail;
        }
        if (!is_valtype(u8)) {
                ret = EINVAL;
                goto fail;
        }
        *vt = u8;
        *pp = p;
        return 0;
fail:
        return ret;
}

int
read_resulttype(const uint8_t **pp, const uint8_t *ep, struct resulttype *rt)
{
        const uint8_t *p = *pp;
        int ret;

        rt->ntypes = 0;
        rt->types = NULL;
        ret = read_vec(&p, ep, sizeof(*rt->types), (void *)read_valtype, NULL,
                       &rt->ntypes, (void *)&rt->types);
        if (ret != 0) {
                goto fail;
        }
        rt->is_static = true;
        *pp = p;
        return 0;
fail:
        return ret;
}

void
clear_resulttype(struct resulttype *rt)
{
        free(rt->types);
}

int
read_functype(const uint8_t **pp, const uint8_t *ep, struct functype *ft)
{
        const uint8_t *p = *pp;
        uint8_t u8;
        int ret;

        ret = read_u8(&p, ep, &u8);
        if (ret != 0) {
                goto fail;
        }
        if (u8 != 0x60) {
                ret = EINVAL;
                goto fail;
        }

        ret = read_resulttype(&p, ep, &ft->parameter);
        if (ret != 0) {
                goto fail;
        }

        ret = read_resulttype(&p, ep, &ft->result);
        if (ret != 0) {
                clear_resulttype(&ft->parameter);
                goto fail;
        }

        *pp = p;
fail:
        return ret;
}

void
clear_functype(struct functype *ft)
{
        clear_resulttype(&ft->parameter);
        clear_resulttype(&ft->result);
}

void
print_resulttype(const struct resulttype *rt)
{
        const char *sep = "";
        uint32_t i;

        xlog_printf_raw("[");
        for (i = 0; i < rt->ntypes; i++) {
                xlog_printf_raw("%s%s", sep, valtype_str(rt->types[i]));
                sep = ", ";
        }
        xlog_printf_raw("]");
}

void
print_functype(uint32_t i, const struct functype *ft)
{
#if 0
        xlog_printf("func type [%" PRIu32 "]: ", i);
        print_resulttype(&ft->parameter);
        xlog_printf_raw(" -> ");
        print_resulttype(&ft->result);
        xlog_printf_raw("\n");
#endif
}

int
read_limits(const uint8_t **pp, const uint8_t *ep, struct limits *lim)
{
        const uint8_t *p = *pp;
        uint8_t u8;
        int ret;

        ret = read_u8(&p, ep, &u8);
        if (ret != 0) {
                goto fail;
        }
        bool has_max;
        switch (u8) {
        case 0x01:
                has_max = true;
                break;
        case 0x00:
                has_max = false;
                break;
        default:
                ret = EINVAL;
                goto fail;
        }

        ret = read_leb_u32(&p, ep, &lim->min);
        if (ret != 0) {
                goto fail;
        }
        if (has_max) {
                ret = read_leb_u32(&p, ep, &lim->max);
                if (ret != 0) {
                        goto fail;
                }
                /* implementation limit */
                if (lim->max == UINT32_MAX) {
                        ret = ENOTSUP;
                        goto fail;
                }
        } else {
                lim->max = UINT32_MAX;
        }
        if (lim->min > lim->max) {
                ret = EINVAL;
                goto fail;
        }

        *pp = p;
fail:
        return ret;
}

int
read_memtype(const uint8_t **pp, const uint8_t *ep, struct limits *lim)
{
        int ret = read_limits(pp, ep, lim);
        if (ret == 0) {
                xlog_trace("mem min %" PRIu32 " max %" PRIu32, lim->min,
                           lim->max);
                if (WASM_MAX_PAGES < lim->min) {
                        ret = EOVERFLOW;
                }
                if (lim->max != UINT32_MAX && WASM_MAX_PAGES < lim->max) {
                        ret = EOVERFLOW;
                }
        }
        return ret;
}

int
read_globaltype(const uint8_t **pp, const uint8_t *ep, struct globaltype *g)
{
        const uint8_t *p = *pp;
        uint8_t u8;
        int ret;

        ret = read_valtype(&p, ep, &g->t);
        if (ret != 0) {
                goto fail;
        }

        ret = read_u8(&p, ep, &u8);
        if (ret != 0) {
                goto fail;
        }
        switch (u8) {
        case 0x01: /* var */
                g->mut = GLOBAL_VAR;
                break;
        case 0x00: /* const */
                g->mut = GLOBAL_CONST;
                break;
        default:
                ret = EINVAL;
                goto fail;
        }

        *pp = p;
fail:
        return ret;
}

int
read_name(const uint8_t **pp, const uint8_t *ep, char **namep)
{
        const uint8_t *p = *pp;
        char *name = NULL;
        uint32_t vec_count;
        int ret;

        ret = read_vec_count(&p, ep, &vec_count);
        if (ret != 0) {
                goto fail;
        }

        /*
         * "name" is not NUL terminated.
         * NUL terminate for our convenience.
         */

        if (vec_count > ep - p) {
                ret = E2BIG;
                goto fail;
        }

        /* TODO: utf-8 validation */

        name = malloc(vec_count + 1);
        if (name == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        memcpy(name, p, vec_count);
        name[vec_count] = 0;
        p += vec_count;

        *pp = p;
        *namep = name;
        return 0;
fail:
        free(name);
        return ret;
}

int
read_tabletype(const uint8_t **pp, const uint8_t *ep, struct tabletype *tt)
{
        const uint8_t *p = *pp;
        int ret;

        ret = read_valtype(&p, ep, &tt->et);
        if (ret != 0) {
                goto fail;
        }
        if (!is_reftype(tt->et)) {
                ret = EINVAL;
                goto fail;
        }
        ret = read_limits(&p, ep, &tt->lim);
        if (ret != 0) {
                goto fail;
        }
        *pp = p;
        return 0;
fail:
        return ret;
}

int
read_importdesc(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
                struct importdesc *desc, struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        uint8_t u8;
        int ret;

        ret = read_u8(&p, ep, &u8);
        if (ret != 0) {
                goto fail;
        }
        switch (u8) {
        case 0x00: /* typeidx */
                ret = read_leb_u32(&p, ep, &desc->u.typeidx);
                if (ret != 0) {
                        goto fail;
                }
                if (desc->u.typeidx >= ctx->module->ntypes) {
                        ret = EINVAL;
                        goto fail;
                }
                m->nimportedfuncs++;
                break;
        case 0x01: /* tabletype */
                ret = read_tabletype(&p, ep, &desc->u.tabletype);
                if (ret != 0) {
                        goto fail;
                }
                m->nimportedtables++;
                break;
        case 0x02: /* memtype */
                ret = read_memtype(&p, ep, &desc->u.memtype.lim);
                if (ret != 0) {
                        goto fail;
                }
                m->nimportedmems++;
                break;
        case 0x03: /* globaltype */
                ret = read_globaltype(&p, ep, &desc->u.globaltype);
                if (ret != 0) {
                        goto fail;
                }
                m->nimportedglobals++;
                break;
        default:
                xlog_trace("unknown import desc type %u", u8);
                ret = EINVAL;
                goto fail;
        }
        desc->type = u8;
        *pp = p;
        return 0;
fail:
        return ret;
}

int
read_exportdesc(const uint8_t **pp, const uint8_t *ep, struct exportdesc *desc,
                struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        uint8_t u8;
        int ret;

        ret = read_u8(&p, ep, &u8);
        if (ret != 0) {
                goto fail;
        }
        switch (u8) {
        case 0x00: /* typeidx */
        case 0x01: /* tableidx */
        case 0x02: /* memidx */
        case 0x03: /* globalidx */
                desc->type = u8;
                ret = read_leb_u32(&p, ep, &desc->idx);
                if (ret != 0) {
                        goto fail;
                }
                break;
        default:
                xlog_trace("unknown export desc type %u", u8);
                ret = EINVAL;
                goto fail;
        }
        switch (desc->type) {
        case EXPORT_FUNC:
                if (desc->idx >= m->nimportedfuncs + m->nfuncs) {
                        xlog_trace("export idx (%" PRIu32
                                   ") out of range for type %u",
                                   desc->idx, u8);
                        ret = EINVAL;
                        goto fail;
                }
                break;
        case EXPORT_TABLE:
                if (desc->idx >= m->nimportedtables + m->ntables) {
                        ret = EINVAL;
                        goto fail;
                }
                break;
        case EXPORT_MEMORY:
                if (desc->idx >= m->nimportedmems + m->nmems) {
                        ret = EINVAL;
                        goto fail;
                }
                break;
        case EXPORT_GLOBAL:
                if (desc->idx >= m->nimportedglobals + m->nglobals) {
                        ret = EINVAL;
                        goto fail;
                }
                break;
        }
        *pp = p;
        return 0;
fail:
        return ret;
}

int
read_import(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
            struct import *im, void *ctx)
{
        const uint8_t *p = *pp;
        char *module_name = NULL;
        char *name = NULL;
        int ret;

        ret = read_name(&p, ep, &module_name);
        if (ret != 0) {
                goto fail;
        }
        ret = read_name(&p, ep, &name);
        if (ret != 0) {
                goto fail;
        }
        ret = read_importdesc(&p, ep, idx, &im->desc, ctx);
        if (ret != 0) {
                goto fail;
        }

        *pp = p;
        im->module_name = module_name;
        im->name = name;
        return 0;
fail:
        free(module_name);
        free(name);
        return ret;
}

void
clear_import(struct import *im)
{
        free(im->module_name);
        free(im->name);
}

int
read_export(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
            struct export *ex, void *vctx)
{
        struct load_context *ctx = vctx;
        const uint8_t *p = *pp;
        char *name = NULL;
        int ret;

        ret = read_name(&p, ep, &name);
        if (ret != 0) {
                goto fail;
        }
        ret = read_exportdesc(&p, ep, &ex->desc, ctx);
        if (ret != 0) {
                goto fail;
        }

        *pp = p;
        ex->name = name;
        return 0;
fail:
        free(name);
        return ret;
}

void
clear_export(struct export *ex)
{
        free(ex->name);
}

void
print_import(const struct import *im)
{
        xlog_trace("import module %s name %s type %u", im->module_name,
                   im->name, im->desc.type);
}

void
print_export(const struct export *ex)
{
        xlog_trace("export name %s type %u idx %" PRIu32, ex->name,
                   ex->desc.type, ex->desc.idx);
}

int
read_type_section(const uint8_t **pp, const uint8_t *ep,
                  struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec(&p, ep, sizeof(*m->types), (void *)read_functype,
                       (void *)clear_functype, &m->ntypes, (void *)&m->types);
        if (ret != 0) {
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->ntypes; i++) {
                print_functype(i, &m->types[i]);
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

int
read_import_section(const uint8_t **pp, const uint8_t *ep,
                    struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_with_ctx2(&p, ep, sizeof(struct import), read_import,
                                 clear_import, ctx, &m->nimports, &m->imports);
        if (ret != 0) {
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->nimports; i++) {
                print_import(&m->imports[i]);
        }
        ret = 0;
        *pp = p;
fail:
        return ret;
}

int
read_locals(const uint8_t **pp, const uint8_t *ep, struct func *func)
{
        const uint8_t *p = *pp;
        uint32_t vec_count;
        int ret;
        struct localchunk *chunks = NULL;
        uint32_t nlocals = 0;

        ret = read_vec_count(&p, ep, &vec_count);
        if (ret != 0) {
                goto fail;
        }
        func->nlocalchunks = vec_count;
        chunks = calloc(vec_count, sizeof(*chunks));

        uint32_t i;
        for (i = 0; i < vec_count; i++) {
                uint32_t count;
                uint32_t old_nlocals;
                uint8_t u8;

                ret = read_leb_u32(&p, ep, &count);
                if (ret != 0) {
                        goto fail;
                }
                old_nlocals = nlocals;
                nlocals += count;
                if (old_nlocals >= nlocals) {
                        ret = E2BIG;
                        goto fail;
                }
                ret = read_u8(&p, ep, &u8);
                if (ret != 0) {
                        goto fail;
                }
                if (!is_valtype(u8)) {
                        ret = EINVAL;
                        goto fail;
                }
                chunks[i].n = count;
                chunks[i].type = u8;
        }
        func->localchunks = chunks;
        func->nlocals = nlocals;
        *pp = p;
        return 0;
fail:
        free(chunks);
        return ret;
}

int
read_func(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
          struct func *func, void *vp)
{
        struct load_context *ctx = vp;
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        uint32_t size;
        int ret;

        func->localchunks = NULL;
        uint32_t funcidx = m->nimportedfuncs + idx;
        if (funcidx >= m->nimportedfuncs + m->nfuncs) {
                xlog_trace("read_func: funcidx out of range");
                ret = EINVAL;
                goto fail;
        }
        uint32_t functypeidx = m->functypeidxes[idx];
        assert(functypeidx < m->ntypes);
        struct functype *ft = &m->types[functypeidx];
        xlog_trace("reading func [%" PRIu32 "] (code [%" PRIu32
                   "], type[%" PRIu32 "])",
                   funcidx, idx, functypeidx);

        /* code */
        ret = read_leb_u32(&p, ep, &size);
        if (ret != 0) {
                goto fail;
        }

        const uint8_t *cep = p + size;
        ret = read_locals(&p, cep, func);
        if (ret != 0) {
                goto fail;
        }
        ret = read_expr(&p, cep, &func->e, func->nlocals, func->localchunks,
                        &ft->parameter, &ft->result, ctx);
        if (ret != 0) {
                goto fail;
        }
        if (p != cep) {
                xlog_trace("func has %zu trailing bytes", cep - p);
                ret = EINVAL;
                goto fail;
        }
        *pp = p;
        return 0;
fail:
        free(func->localchunks);
        return ret;
}

void
clear_expr_exec_info(struct expr_exec_info *ei)
{
        free(ei->jumps);
}

void
clear_expr(struct expr *expr)
{
        clear_expr_exec_info(&expr->ei);
}

void
clear_func(struct func *func)
{
        free(func->localchunks);
        clear_expr(&func->e);
}

int
read_function_section(const uint8_t **pp, const uint8_t *ep,
                      struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_u32(&p, ep, &m->nfuncs, &m->functypeidxes);
        if (ret != 0) {
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->nfuncs; i++) {
                if (m->functypeidxes[i] >= m->ntypes) {
                        xlog_trace("func type idx out of range");
                        return EINVAL;
                }
                xlog_trace("func [%" PRIu32 "] typeidx %" PRIu32, i,
                           m->functypeidxes[i]);
        }
        if (m->nimportedfuncs + m->nfuncs > 0) {
                ret = bitmap_alloc(&ctx->refs, m->nimportedfuncs + m->nfuncs);
                if (ret != 0) {
                        goto fail;
                }
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

int
read_table_section(const uint8_t **pp, const uint8_t *ep,
                   struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec(&p, ep, sizeof(*m->tables), (void *)read_tabletype,
                       NULL, &m->ntables, (void *)&m->tables);
        if (ret != 0) {
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->ntables; i++) {
                xlog_trace("table [%" PRIu32 "] %s %" PRIu32 " - %" PRIu32, i,
                           valtype_str(m->tables[i].et), m->tables[i].lim.min,
                           m->tables[i].lim.max);
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

int
read_memory_section(const uint8_t **pp, const uint8_t *ep,
                    struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec(&p, ep, sizeof(*m->mems), (void *)read_memtype, NULL,
                       &m->nmems, (void *)&m->mems);
        if (ret != 0) {
                xlog_trace("failed to load mems with %d", ret);
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->nmems; i++) {
                xlog_trace("mem [%" PRIu32 "] %" PRIu32 " - %" PRIu32, i,
                           m->mems[i].min, m->mems[i].max);
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

int
read_global(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
            struct global *g, void *vctx)
{
        struct load_context *ctx = vctx;
        const uint8_t *p = *pp;
        int ret;

        ret = read_globaltype(&p, ep, &g->type);
        if (ret != 0) {
                goto fail;
        }
        ret = read_const_expr(&p, ep, &g->init, g->type.t, ctx);
        if (ret != 0) {
                goto fail;
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

void
clear_global(struct global *g)
{
        clear_expr(&g->init);
}

int
read_global_section(const uint8_t **pp, const uint8_t *ep,
                    struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_with_ctx2(&p, ep, sizeof(*m->globals), read_global,
                                 clear_global, ctx, &m->nglobals, &m->globals);
        if (ret != 0) {
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->nglobals; i++) {
                xlog_trace("global [%" PRIu32 "] %s %s", i,
                           valtype_str(m->globals[i].type.t),
                           m->globals[i].type.mut ? "var" : "const");
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

int
read_export_section(const uint8_t **pp, const uint8_t *ep,
                    struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_with_ctx2(&p, ep, sizeof(struct export), read_export,
                                 clear_export, ctx, &m->nexports, &m->exports);
        if (ret != 0) {
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->nexports; i++) {
                print_export(&m->exports[i]);
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

int
read_start_section(const uint8_t **pp, const uint8_t *ep,
                   struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_leb_u32(&p, ep, &m->start);
        if (ret != 0) {
                goto fail;
        }
        m->has_start = true;
        if (m->start >= m->nimportedfuncs + m->nfuncs) {
                ret = EINVAL;
                goto fail;
        }
        const struct functype *ft = module_functype(m, m->start);
        if (ft->parameter.ntypes > 0 || ft->result.ntypes > 0) {
                ret = EINVAL;
                goto fail;
        }

        xlog_trace("start %" PRIu32, m->start);

        ret = 0;
        *pp = p;
fail:
        return ret;
}

struct read_element_init_expr_context {
        struct load_context *lctx;
        struct element *elem;
};

int
read_element_init_expr(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
                       struct expr *e, void *vctx)
{
        struct read_element_init_expr_context *ctx = vctx;
        struct element *elem = ctx->elem;
        /*
         * TODO this should be more restrictive than other const expr
         * https://github.com/WebAssembly/spec/blob/main/proposals/bulk-memory-operations/Overview.md#element-segments
         */
        return read_const_expr(pp, ep, e, elem->type, ctx->lctx);
}

/*
 * https://webassembly.github.io/spec/core/binary/modules.html#element-section
 * https://webassembly.github.io/spec/core/valid/modules.html#element-segments
 */
int
read_element(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
             struct element *elem, void *vctx)
{
        struct load_context *ctx = vctx;
        struct module *module = ctx->module;
        const uint8_t *p = *pp;
        uint32_t u32;
        uint32_t tableidx;
        enum valtype et;
        uint8_t u8;
        struct read_element_init_expr_context init_expr_ctx = {
                .lctx = ctx,
                .elem = elem,
        };
        uint32_t i;
        int ret;

        memset(elem, 0, sizeof(*elem));
        ret = read_leb_u32(&p, ep, &u32);
        if (ret != 0) {
                goto fail;
        }
        if (u32 > 7) {
                report_error(&ctx->report, "unimplemented element %" PRIu32,
                             u32);
                ret = EINVAL;
                goto fail;
        }
        switch (u32) {
        case 2:
        case 6:
                /* tableidx */
                ret = read_leb_u32(&p, ep, &tableidx);
                if (ret != 0) {
                        goto fail;
                }
                elem->table = tableidx;
                break;
        default:
                /*
                 * 0, 4 -> tableidx 0 is implicit
                 *
                 * others -> non-active, no tableidx here
                 */
                elem->table = 0;
                break;
        }
        switch (u32) {
        case 0:
        case 2:
        case 4:
        case 6:
                elem->mode = ELEM_MODE_ACTIVE;
                /* offset */
                ret = read_const_expr(&p, ep, &elem->offset, TYPE_i32, ctx);
                if (ret != 0) {
                        goto fail;
                }
                break;
        case 1:
        case 5:
                elem->mode = ELEM_MODE_PASSIVE;
                break;
        default:
                elem->mode = ELEM_MODE_DECLARATIVE;
                break;
        }
        switch (u32) {
        case 1:
        case 2:
        case 3:
                /* elemkind */
                ret = read_u8(&p, ep, &u8);
                if (ret != 0) {
                        goto fail;
                }
                if (u8 != 0) {
                        report_error(&ctx->report, "unexpected elemkind %u",
                                     u8);
                        ret = EINVAL;
                        goto fail;
                }
                /* fallthrough */
        default: /* 0, 4 */
                elem->type = TYPE_FUNCREF;
                break;
        case 5:
        case 6:
        case 7:
                /* reftype */
                ret = read_u8(&p, ep, &u8);
                if (ret != 0) {
                        goto fail;
                }
                et = u8;
                if (!is_reftype(et)) {
                        report_error(&ctx->report, "unexpected reftype %u",
                                     u8);
                        ret = EINVAL;
                        goto fail;
                }
                elem->type = et;
                break;
        }
        switch (u32) {
        case 0:
        case 1:
        case 2:
        case 3:
                ret = read_vec_u32(&p, ep, &elem->init_size, &elem->funcs);
                if (ret != 0) {
                        goto fail;
                }
                for (i = 0; i < elem->init_size; i++) {
                        if (elem->funcs[i] >= ctx->module->nimportedfuncs +
                                                      ctx->module->nfuncs) {
                                ret = EINVAL;
                                goto fail;
                        }
                        bitmap_set(&ctx->refs, elem->funcs[i]);
                }
                break;
        case 4:
        case 5:
        case 6:
        case 7:
                /*
                 * vec(expr)
                 */
                ret = read_vec_with_ctx2(&p, ep, sizeof(*elem->init_exprs),
                                         read_element_init_expr, clear_expr,
                                         &init_expr_ctx, &elem->init_size,
                                         &elem->init_exprs);
                if (ret != 0) {
                        goto fail;
                }
        }
        if (elem->mode == ELEM_MODE_ACTIVE) {
                if (elem->table >= module->nimportedtables + module->ntables) {
                        report_error(&ctx->report,
                                     "element tableidx out of range");
                        ret = EINVAL;
                        goto fail;
                }
                enum valtype table_type =
                        module_tabletype(module, elem->table)->et;
                if (table_type != elem->type) {
                        report_error(&ctx->report,
                                     "element type mismatch %u != %u",
                                     table_type, elem->type);
                        ret = EINVAL;
                        goto fail;
                }
        }
        ret = 0;
        *pp = p;
        return 0;
fail:
        return ret;
}

void
clear_element(struct element *elem)
{
        if (elem->init_exprs != NULL) {
                uint32_t i;
                for (i = 0; i < elem->init_size; i++) {
                        clear_expr(&elem->init_exprs[i]);
                }
                free(elem->init_exprs);
        }
        free(elem->funcs);
        clear_expr(&elem->offset);
}
void
print_element(uint32_t idx, const struct element *elem)
{
        xlog_trace("element [%" PRIu32 "] %s", idx, valtype_str(elem->type));
}

int
read_element_section(const uint8_t **pp, const uint8_t *ep,
                     struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_with_ctx2(&p, ep, sizeof(*m->elems), read_element,
                                 clear_element, ctx, &m->nelems, &m->elems);
        if (ret != 0) {
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->nelems; i++) {
                print_element(i, &m->elems[i]);
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

int
read_code_section(const uint8_t **pp, const uint8_t *ep,
                  struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        assert(m->funcs == NULL);
        uint32_t nfuncs_in_code = 0;
        ret = read_vec_with_ctx2(&p, ep, sizeof(*m->funcs), read_func,
                                 clear_func, ctx, &nfuncs_in_code, &m->funcs);
        if (ret != 0) {
                assert(nfuncs_in_code == 0);
                goto fail;
        }
        if (nfuncs_in_code != m->nfuncs) {
                xlog_trace("nfunc mismatch %" PRIu32 " != %" PRIu32,
                           nfuncs_in_code, m->nfuncs);
                m->nfuncs = nfuncs_in_code; /* for unload */
                ret = EINVAL;
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->nfuncs; i++) {
                xlog_trace("func nlocals %u", m->funcs[i].nlocals);
        }

        *pp = p;
        return 0;
fail:
        xlog_trace("read_code_section failed");
        /* avoid accessing uninitializing data in module_unload */
        free(m->funcs);
        m->funcs = NULL;
        return ret;
}

int
read_data(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
          struct data *data, void *vctx)
{
        struct load_context *ctx = vctx;
        const uint8_t *p = *pp;
        uint32_t u32;
        int ret;

        memset(data, 0, sizeof(*data));
        ret = read_leb_u32(&p, ep, &u32);
        if (ret != 0) {
                goto fail;
        }
        data->memory = 0;
        struct module *m = ctx->module;
        switch (u32) {
        case 1:
                data->mode = DATA_MODE_PASSIVE;
                break;
        case 2:
                ret = read_leb_u32(&p, ep, &data->memory);
                if (ret != 0) {
                        goto fail;
                }
                /* fallthrough */
        case 0:
                data->mode = DATA_MODE_ACTIVE;
                if (data->memory >= m->nimportedmems + m->nmems) {
                        ret = EINVAL;
                        goto fail;
                }
                ret = read_const_expr(&p, ep, &data->offset, TYPE_i32, ctx);
                if (ret != 0) {
                        goto fail;
                }
                break;
        default:
                report_error(&ctx->report, "unknown data %" PRIu32, u32);
                ret = EINVAL;
                goto fail;
        }

        uint32_t vec_count;
        ret = read_vec_count(&p, ep, &vec_count);
        if (ret != 0) {
                goto fail;
        }
        if (ep - p < vec_count) {
                ret = E2BIG;
                goto fail;
        }
        data->init_size = vec_count;
        data->init = p;
        p += vec_count;

        ret = 0;
        *pp = p;
        return 0;
fail:
        return ret;
}

void
clear_data(struct data *data)
{
        clear_expr(&data->offset);
}

void
print_data(uint32_t idx, const struct data *data)
{
        xlog_trace("data [%" PRIu32 "] %" PRIu32 " bytes", idx,
                   data->init_size);
}

int
read_data_section(const uint8_t **pp, const uint8_t *ep,
                  struct load_context *ctx)
{
        struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;

        ret = read_vec_with_ctx2(&p, ep, sizeof(*m->datas), read_data,
                                 clear_data, ctx, &m->ndatas, &m->datas);
        if (ret != 0) {
                goto fail;
        }

        uint32_t i;
        for (i = 0; i < m->ndatas; i++) {
                print_data(i, &m->datas[i]);
        }

        ret = 0;
        *pp = p;
fail:
        return ret;
}

int
read_datacount_section(const uint8_t **pp, const uint8_t *ep,
                       struct load_context *ctx)
{
        const uint8_t *p = *pp;
        uint32_t count;
        int ret;
        ret = read_leb_u32(&p, ep, &count);
        if (ret != 0) {
                goto fail;
        }
        ctx->has_datacount = true;
        ctx->ndatas_in_datacount = count;
        ret = 0;
        *pp = ep;
fail:
        return ret;
}

int
read_custom_section(const uint8_t **pp, const uint8_t *ep,
                    struct load_context *ctx)
{
        const uint8_t *p = *pp;
        uint32_t name_len;
        int ret;
        ret = read_leb_u32(&p, ep, &name_len);
        if (ret != 0) {
                goto fail;
        }
        if (ep - p < name_len) {
                ret = EINVAL;
                goto fail;
        }
        if (name_len < INT_MAX) {
                xlog_trace("custom section name %.*s", (int)name_len, p);
        }
        p += name_len;
        /*
         * unspecified bytes follow. just skip them.
         */
        ret = 0;
        *pp = ep;
fail:
        return ret;
}

struct section_type {
        const char *name;
        int (*read)(const uint8_t **pp, const uint8_t *ep,
                    struct load_context *ctx);
        int order;
};

/*
 * https://webassembly.github.io/spec/core/binary/modules.html#binary-typeidx
 */

#define SECTION(id, n, o)                                                     \
        [id] = {.name = #n, .read = read_##n##_section, .order = o}
#define SECTION_NOOP(id, n, o) [id] = {.name = #n, .read = NULL, .order = o}

const struct section_type section_types[] = {
        SECTION(0, custom, 0),      SECTION(1, type, 1),
        SECTION(2, import, 2),      SECTION(3, function, 3),
        SECTION(4, table, 4),       SECTION(5, memory, 5),
        SECTION(6, global, 6),      SECTION(7, export, 7),
        SECTION(8, start, 8),       SECTION(9, element, 9),
        SECTION(10, code, 11),      SECTION(11, data, 12),
        SECTION(12, datacount, 10),
};

const struct section_type *
get_section_type(uint8_t id)
{
        if (id >= ARRAYCOUNT(section_types)) {
                return NULL;
        }
        return &section_types[id];
}

int
module_load(struct module *m, const uint8_t *p, const uint8_t *ep,
            struct load_context *ctx)
{
        uint32_t v;
        int ret;

        memset(m, 0, sizeof(*m));
        ctx->module = m;
        m->bin = p;

        ret = read_u32(&p, ep, &v);
        if (ret != 0) {
                goto fail;
        }
        if (v != 0x6d736100) { /* magic */
                xlog_trace("wrong magic: %" PRIx32, v);
                ret = EINVAL;
                goto fail;
        }

        ret = read_u32(&p, ep, &v);
        if (ret != 0) {
                goto fail;
        }
        if (v != 1) { /* version */
                xlog_trace("wrong version: %u", v);
                ret = EINVAL;
                goto fail;
        }

        uint8_t max_seen_section_id = 0;
        while (p < ep) {
                struct section s;
                ret = section_load(&s, &p, ep);
                if (ret != 0) {
                        goto fail;
                }
                const struct section_type *t = get_section_type(s.id);

                if (t == NULL) {
                        xlog_trace("unknown section %u", s.id);
                        ret = EINVAL;
                        goto fail;
                }
#if defined(ENABLE_TRACING)
                const char *name = t->name;
#endif
                /*
                 * sections except the custom section (id=0) should be
                 * seen in order, at most once.
                 */
                if (s.id > 0) {
                        if (max_seen_section_id >= t->order) {
                                xlog_trace("unexpected section %u (%s)", s.id,
                                           name);
                                ret = EINVAL;
                                goto fail;
                        }
                        max_seen_section_id = t->order;
                }
                xlog_trace("section %u (%s), size %" PRIu32, s.id, name,
                           s.size);
                if (t != NULL && t->read != NULL) {
                        const uint8_t *sp = s.data;
                        const uint8_t *sep = sp + s.size;

                        ret = t->read(&sp, sep, ctx);
                        if (ret != 0) {
                                goto fail;
                        }
                        if (sp != sep) {
                                xlog_trace("section (%s) has %zu bytes extra "
                                           "data",
                                           name, sep - sp);
                                ret = EINVAL;
                                goto fail;
                        }
                }
        }

        /*
         * TODO some of module validations probably need to be done here
         * https://webassembly.github.io/spec/core/valid/modules.html
         */

        if (m->nimportedmems + m->nmems > 1) {
                ret = EINVAL;
                goto fail;
        }

        if ((m->funcs == NULL) != (m->nfuncs == 0)) {
                /* maybe there was no code section */
                ret = EINVAL;
                goto fail;
        }

        if (ctx->has_datacount && ctx->ndatas_in_datacount != m->ndatas) {
                ret = EINVAL;
                goto fail;
        }

        /*
         * export names should be unique.
         * TODO use something which is not O(n^2)
         */
        uint32_t i;
        for (i = 0; i < m->nexports; i++) {
                uint32_t j;
                for (j = i + 1; j < m->nexports; j++) {
                        if (!strcmp(m->exports[i].name, m->exports[j].name)) {
                                ret = EINVAL;
                                goto fail;
                        }
                }
        }

        /*
         * Note: This validation is a bit special because the number of
         * data segments is not known when we validate instructions.
         *
         * See the note in:
         * https://webassembly.github.io/spec/core/binary/modules.html#data-count-section
         */
        if (ctx->expected_ndatas > m->ndatas) {
                report_error(&ctx->report, "dataidx validation error");
                ret = EINVAL;
                goto fail;
        }
        ret = 0;
fail:
        return ret;
}

void
module_unload(struct module *m)
{
        uint32_t i;

        for (i = 0; i < m->ntypes; i++) {
                clear_functype(&m->types[i]);
        }
        free(m->types);

        if (m->funcs != NULL) {
                for (i = 0; i < m->nfuncs; i++) {
                        clear_func(&m->funcs[i]);
                }
                free(m->funcs);
        }
        free(m->functypeidxes);

        free(m->tables);
        free(m->mems);

        for (i = 0; i < m->nglobals; i++) {
                clear_global(&m->globals[i]);
        }
        free(m->globals);

        for (i = 0; i < m->nelems; i++) {
                clear_element(&m->elems[i]);
        }
        free(m->elems);

        for (i = 0; i < m->ndatas; i++) {
                clear_data(&m->datas[i]);
        }
        free(m->datas);

        for (i = 0; i < m->nimports; i++) {
                clear_import(&m->imports[i]);
        }
        free(m->imports);

        for (i = 0; i < m->nexports; i++) {
                clear_export(&m->exports[i]);
        }
        free(m->exports);

        memset(m, 0, sizeof(*m));
}

int
module_create(struct module **mp)
{
        struct module *m = zalloc(sizeof(*m));
        if (m == NULL) {
                return ENOMEM;
        }
        *mp = m;
        return 0;
}

void
module_destroy(struct module *m)
{
        assert(m != NULL);
        module_unload(m);
        free(m);
}

void
load_context_init(struct load_context *ctx)
{
        memset(ctx, 0, sizeof(*ctx));
        report_init(&ctx->report);
}

void
load_context_clear(struct load_context *ctx)
{
        report_clear(&ctx->report);
        bitmap_free(&ctx->refs);
}
