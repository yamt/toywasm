#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

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
        case TYPE_v128:
                return true;
        default:
                break;
        }
        return false;
}

bool
is_reftype(enum valtype vt)
{
        switch (vt) {
        case TYPE_FUNCREF:
        case TYPE_EXTERNREF:
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
module_find_export_func(struct module *m, const struct name *name,
                        uint32_t *funcidxp)
{
        uint32_t i;
        for (i = 0; i < m->nexports; i++) {
                const struct export *ex = &m->exports[i];
                if (!compare_name(&ex->name, name)) {
                        const struct exportdesc *exd = &ex->desc;
                        if (exd->type == EXPORT_FUNC) {
                                *funcidxp = exd->idx;
                                return 0;
                        }
                }
        }
        return ENOENT;
}

const struct import *
module_find_import(const struct module *m, enum importtype type, uint32_t idx)
{
        /*
         * REVISIT: this is O(n) and thus some of users
         * (eg. instance_create) are O(n^2).
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
module_find_importdesc(const struct module *m, enum importtype type,
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
                functypeidx =
                        module_find_importdesc(m, IMPORT_FUNC, idx)->u.typeidx;
        } else {
                functypeidx = m->functypeidxes[idx - m->nimportedfuncs];
        }
        assert(functypeidx < m->ntypes);
        return &m->types[functypeidx];
}

const struct limits *
module_memtype(const struct module *m, uint32_t idx)
{
        if (idx < m->nimportedmems) {
                return &module_find_importdesc(m, IMPORT_MEMORY, idx)
                                ->u.memtype.lim;
        }
        return &m->mems[idx - m->nimportedmems];
}

const struct tabletype *
module_tabletype(const struct module *m, uint32_t idx)
{
        if (idx < m->nimportedtables) {
                return &module_find_importdesc(m, IMPORT_TABLE, idx)
                                ->u.tabletype;
        }
        return &m->tables[idx - m->nimportedtables];
}

const struct globaltype *
module_globaltype(const struct module *m, uint32_t idx)
{
        if (idx < m->nimportedglobals) {
                return &module_find_importdesc(m, IMPORT_GLOBAL, idx)
                                ->u.globaltype;
        }
        return &m->globals[idx - m->nimportedglobals].type;
}

const struct functype *
funcinst_functype(const struct funcinst *fi)
{
        if (fi->is_host) {
                return fi->u.host.type;
        }
        return module_functype(fi->u.wasm.instance->module,
                               fi->u.wasm.funcidx);
}

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
                return 1;
        }
        return memcmp(a->data, b->data, a->nbytes);
}

static int
resulttype_from_string(const char *p, const char *ep, struct resulttype *t)
{
        size_t ntypes = ep - p;
        int ret;
        if (ntypes > UINT32_MAX) {
                return EOVERFLOW;
        }
        t->ntypes = ntypes;
        t->types = malloc(ntypes * sizeof(*t->types));
        if (t->types == NULL) {
                return ENOMEM;
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
        clear_resulttype(t);
        t->types = NULL;
        return ret;
}

void
functype_free(struct functype *ft)
{
        clear_functype(ft);
        free(ft);
}

int
functype_from_string(const char *p, struct functype **resultp)
{
        struct functype *ft;
        int ret;
        ft = zalloc(sizeof(*ft));
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
        ret = resulttype_from_string(p, ep, &ft->parameter);
        if (ret != 0) {
                goto fail;
        }
        p = ep + 1;
        ep = strchr(p, 0);
        assert(ep != NULL);
        ret = resulttype_from_string(p, ep, &ft->result);
        if (ret != 0) {
                goto fail;
        }
        *resultp = ft;
        return 0;
fail:
        functype_free(ft);
        return ret;
}
