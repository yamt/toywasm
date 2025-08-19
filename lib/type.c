#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "type.h"
#include "xlog.h"

bool
is_numtype(enum valtype vt)
{
        switch (vt) {
        case TYPE_i32:
        case TYPE_i64:
        case TYPE_f32:
        case TYPE_f64:
                return true;
        default:
                break;
        }
        return false;
}

bool
is_vectype(enum valtype vt)
{
        switch (vt) {
#if defined(TOYWASM_ENABLE_WASM_SIMD)
        case TYPE_v128:
                return true;
#endif
        default:
                break;
        }
        return false;
}

bool
is_reftype(enum valtype vt)
{
        switch (vt) {
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        case TYPE_exnref:
#endif
        case TYPE_funcref:
        case TYPE_externref:
                return true;
        default:
                break;
        }
        return false;
}

bool
is_valtype(enum valtype vt)
{
        return is_numtype(vt) || is_vectype(vt) || is_reftype(vt);
}

int
module_find_export(const struct module *m, const struct name *name,
                   uint32_t type, uint32_t *idxp)
{
#if defined(TOYWASM_SORT_EXPORTS)
        /*
         * binary search
         * O(lg(n)) where n is the number of exports
         */
        uint32_t left = 0;
        uint32_t right = m->nexports;
        while (left < right) {
                uint32_t mid = left + (right - left) / 2;
                const struct wasm_export *ex = &m->exports[mid];
                int cmp = compare_name(&ex->name, name);
                if (cmp == 0) {
                        const struct exportdesc *exd = &ex->desc;
                        if (exd->type == type) {
                                *idxp = exd->idx;
                                return 0;
                        }
                        break;
                } else if (cmp < 0) {
                        left = mid + 1;
                } else {
                        right = mid;
                }
        }
        return ENOENT;
#else
        /*
         * linear search
         * O(n) where n is the number of exports
         */
        uint32_t i;
        for (i = 0; i < m->nexports; i++) {
                const struct wasm_export *ex = &m->exports[i];
                if (!compare_name(&ex->name, name)) {
                        const struct exportdesc *exd = &ex->desc;
                        if (exd->type == type) {
                                *idxp = exd->idx;
                                return 0;
                        }
                }
        }
        return ENOENT;
#endif
}

const struct import *
module_find_import(const struct module *m, enum externtype type, uint32_t idx)
{
        /*
         * REVISIT: this is O(n) and can make some of users O(n^2).
         */
        uint32_t i;
        for (i = 0; i < m->nimports; i++) {
                const struct import *im = &m->imports[i];
                if (im->desc.type != type) {
                        continue;
                }
                if (idx == 0) {
                        return im;
                }
                idx--;
        }
        assert(false);
        return NULL;
}

const struct importdesc *
module_find_importdesc(const struct module *m, enum externtype type,
                       uint32_t idx)
{
        const struct import *im = module_find_import(m, type, idx);
        assert(im != NULL);
        return &im->desc;
}

const struct functype *
module_functype(const struct module *m, uint32_t idx)
{
        uint32_t functypeidx;
        if (idx < m->nimportedfuncs) {
                functypeidx = module_find_importdesc(m, EXTERNTYPE_FUNC, idx)
                                      ->u.typeidx;
        } else {
                functypeidx = m->functypeidxes[idx - m->nimportedfuncs];
        }
        assert(functypeidx < m->ntypes);
        return &m->types[functypeidx];
}

const struct memtype *
module_memtype(const struct module *m, uint32_t idx)
{
        if (idx < m->nimportedmems) {
                return &module_find_importdesc(m, EXTERNTYPE_MEMORY, idx)
                                ->u.memtype;
        }
        return &m->mems[idx - m->nimportedmems];
}

const struct tabletype *
module_tabletype(const struct module *m, uint32_t idx)
{
        if (idx < m->nimportedtables) {
                return &module_find_importdesc(m, EXTERNTYPE_TABLE, idx)
                                ->u.tabletype;
        }
        return &m->tables[idx - m->nimportedtables];
}

const struct globaltype *
module_globaltype(const struct module *m, uint32_t idx)
{
        if (idx < m->nimportedglobals) {
                return &module_find_importdesc(m, EXTERNTYPE_GLOBAL, idx)
                                ->u.globaltype;
        }
        return &m->globals[idx - m->nimportedglobals].type;
}

#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
const struct tagtype *
module_tagtype(const struct module *m, uint32_t idx)
{
        if (idx < m->nimportedtags) {
                return &module_find_importdesc(m, EXTERNTYPE_TAG, idx)
                                ->u.tagtype;
        }
        return &m->tags[idx - m->nimportedtags];
}

const struct functype *
module_tagtype_functype(const struct module *m, const struct tagtype *tt)
{
        uint32_t functypeidx = tt->typeidx;
        assert(functypeidx < m->ntypes);
        const struct functype *ft = &m->types[functypeidx];
        assert(ft->result.ntypes == 0);
        return ft;
}
#endif

const struct functype *
funcinst_functype(const struct funcinst *fi)
{
        if (fi->is_host) {
                return fi->u.host.type;
        }
        return module_functype(fi->u.wasm.instance->module,
                               fi->u.wasm.funcidx);
}

#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
const struct functype *
taginst_functype(const struct taginst *ti)
{
        return ti->type;
}
#endif

int
compare_resulttype(const struct resulttype *a, const struct resulttype *b)
{
        if (a->ntypes != b->ntypes) {
                return 1;
        }
        uint32_t i;
        for (i = 0; i < a->ntypes; i++) {
                if (a->types[i] != b->types[i]) {
                        return 1;
                }
        }
        return 0;
}

int
compare_functype(const struct functype *a, const struct functype *b)
{
        if (compare_resulttype(&a->parameter, &b->parameter)) {
                return 1;
        }
        return compare_resulttype(&a->result, &b->result);
}

int
compare_name(const struct name *a, const struct name *b)
{
        if (a->nbytes != b->nbytes) {
                if (a->nbytes < b->nbytes) {
                        return -1;
                }
                return 1;
        }
        return memcmp(a->data, b->data, a->nbytes);
}

static int
resulttype_from_string(struct mem_context *mctx, const char *p, const char *ep,
                       struct resulttype *t)
{
        size_t ntypes = ep - p;
        int ret;
        if (ntypes > UINT32_MAX) {
                return EOVERFLOW;
        }
        t->ntypes = ntypes;
        if (ntypes > 0) {
                t->types = mem_alloc(mctx, ntypes * sizeof(*t->types));
                if (t->types == NULL) {
                        return ENOMEM;
                }
        } else {
                t->types = NULL;
        }
        size_t i;
        for (i = 0; i < ntypes; i++) {
                switch (p[i]) {
                case 'i':
                        t->types[i] = TYPE_i32;
                        break;
                case 'I':
                        t->types[i] = TYPE_i64;
                        break;
                case 'f':
                        t->types[i] = TYPE_f32;
                        break;
                case 'F':
                        t->types[i] = TYPE_f64;
                        break;
                default:
                        xlog_trace("unimplemented type %c", p[i]);
                        ret = EINVAL;
                        goto fail;
                }
        }
        return 0;
fail:
        clear_resulttype(mctx, t);
        t->types = NULL;
        return ret;
}

void
functype_free(struct mem_context *mctx, struct functype *ft)
{
        clear_functype(mctx, ft);
        mem_free(mctx, ft, sizeof(*ft));
}

int
functype_from_string(struct mem_context *mctx, const char *p,
                     struct functype **resultp)
{
        struct functype *ft;
        int ret;
        ft = mem_zalloc(mctx, sizeof(*ft));
        if (ft == NULL) {
                return ENOMEM;
        }
        if (p[0] != '(') {
                ret = EINVAL;
                goto fail;
        }
        p++;
        const char *ep = strchr(p, ')');
        if (ep == NULL) {
                ret = EINVAL;
                goto fail;
        }
        ret = resulttype_from_string(mctx, p, ep, &ft->parameter);
        if (ret != 0) {
                goto fail;
        }
        p = ep + 1;
        ep = strchr(p, 0);
        assert(ep != NULL);
        ret = resulttype_from_string(mctx, p, ep, &ft->result);
        if (ret != 0) {
                goto fail;
        }
        *resultp = ft;
        return 0;
fail:
        functype_free(mctx, ft);
        return ret;
}

int
check_functype_with_string(const struct module *m, uint32_t funcidx,
                           const char *sig)
{
        const struct functype *ft = module_functype(m, funcidx);
        struct mem_context mctx;
        struct functype *sig_ft;
        int ret;

        mem_context_init(&mctx);
        ret = functype_from_string(&mctx, sig, &sig_ft);
        if (ret != 0) {
                goto fail;
        }
        ret = 0;
        if (compare_functype(ft, sig_ft)) {
                ret = EINVAL;
        }
        functype_free(&mctx, sig_ft);
fail:
        mem_context_clear(&mctx);
        return ret;
}

static char
valtype_to_char(enum valtype t)
{
        switch (t) {
        case TYPE_i32:
                return 'i';
        case TYPE_i64:
                return 'I';
        case TYPE_f32:
                return 'f';
        case TYPE_f64:
                return 'F';
        case TYPE_v128:
                return 'v';
        case TYPE_exnref:
                return 'x';
        case TYPE_funcref:
                return 'c';
        case TYPE_externref:
                return 'e';
        default:
                break;
        }
        xassert(false);
        return 'X';
}

static void
resulttype_to_string(char *p, const struct resulttype *rt)
{
        uint32_t i;
        for (i = 0; i < rt->ntypes; i++) {
                p[i] = valtype_to_char(rt->types[i]);
        }
}

int
functype_to_string(char **pp, const struct functype *ft)
{
        size_t len = 1 + ft->parameter.ntypes + 1 + ft->result.ntypes + 1;
        char *p = malloc(len);
        if (p == NULL) {
                return ENOMEM;
        }
        *pp = p;
        *p++ = '(';
        resulttype_to_string(p, &ft->parameter);
        p += ft->parameter.ntypes;
        *p++ = ')';
        resulttype_to_string(p, &ft->result);
        p += ft->result.ntypes;
        *p = 0;
        return 0;
}

void
functype_string_free(char *p)
{
        free(p);
}

uint32_t
memtype_page_shift(const struct memtype *type)
{
#if defined(TOYWASM_ENABLE_WASM_CUSTOM_PAGE_SIZES)
        return type->page_shift;
#else
        return WASM_PAGE_SHIFT;
#endif
}

uint32_t
memtype_page_size(const struct memtype *type)
{
        return UINT32_C(1) << memtype_page_shift(type);
}
