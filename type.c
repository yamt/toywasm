#include <assert.h>
#include <errno.h>
#include <string.h>

#include "type.h"

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
module_find_export_func(struct module *m, const char *name, uint32_t *funcidxp)
{
        uint32_t i;
        for (i = 0; i < m->nexports; i++) {
                const struct export *ex = &m->exports[i];
                if (!strcmp(ex->name, name)) {
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
