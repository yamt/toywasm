#define _DARWIN_C_SOURCE /* snprintf */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dyld.h"
#include "dyld_plt.h"
#include "dylink_type.h"
#include "fileio.h"
#include "instance.h"
#include "list.h"
#include "load_context.h"
#include "module.h"
#include "type.h"
#include "util.h"
#include "xlog.h"

int dyld_load_object_from_file(struct dyld *d, const struct name *name,
                               const char *filename);

static const struct name name_GOT_mem = NAME_FROM_CSTR_LITERAL("GOT.mem");
static const struct name name_GOT_func = NAME_FROM_CSTR_LITERAL("GOT.func");
static const struct name name_env = NAME_FROM_CSTR_LITERAL("env");
static const struct name name_table_base =
        NAME_FROM_CSTR_LITERAL("__table_base");
static const struct name name_memory_base =
        NAME_FROM_CSTR_LITERAL("__memory_base");
static const struct name name_stack_pointer =
        NAME_FROM_CSTR_LITERAL("__stack_pointer");

static const struct globaltype globaltype_i32_mut = {
        .t = TYPE_i32,
        .mut = GLOBAL_VAR,
};

static const struct globaltype globaltype_i32_const = {
        .t = TYPE_i32,
        .mut = GLOBAL_CONST,
};

static uint32_t
align_up(uint32_t v, uint32_t palign)
{
        uint32_t align = 1 << palign;
        uint32_t mask = align - 1;
        return (v + mask) & ~mask;
}

static uint32_t
global_get_i32(struct globalinst *ginst)
{
        struct val val;
        global_get(ginst, &val);
        return val.u.i32;
}

static void
global_set_i32(struct globalinst *ginst, uint32_t v)
{
        struct val val;
        val.u.i32 = v;
        global_set(ginst, &val);
}

static bool
is_global_type_i32_const(const struct globaltype *gt)
{
        return gt->mut == GLOBAL_CONST && gt->t == TYPE_i32;
}

static bool
is_global_type_i32_mut(const struct globaltype *gt)
{
        return gt->mut == GLOBAL_VAR && gt->t == TYPE_i32;
}

static bool
is_global_i32_mut_import(const struct module *m, const struct import *im)
{
        if (im->desc.type != IMPORT_GLOBAL) {
                return false;
        }
        const struct globaltype *gt = &im->desc.u.globaltype;
        return is_global_type_i32_mut(gt);
}

static bool
is_GOT_mem_import(const struct module *m, const struct import *im)
{
        if (!is_global_i32_mut_import(m, im)) {
                return false;
        }
        if (compare_name(&name_GOT_mem, &im->module_name)) {
                return false;
        }
        return true;
}

static bool
is_GOT_func_import(const struct module *m, const struct import *im)
{
        if (!is_global_i32_mut_import(m, im)) {
                return false;
        }
        if (compare_name(&name_GOT_func, &im->module_name)) {
                return false;
        }
        return true;
}

static bool
is_env_func_import(const struct module *m, const struct import *im)
{
        return im->desc.type == IMPORT_FUNC &&
               !compare_name(&name_env, &im->module_name);
}

static bool
is_func_export(const struct module *m, const struct export *ex)
{
        return ex->desc.type == EXPORT_FUNC;
}

static bool
is_global_export(const struct module *m, const struct export *ex)
{
        if (ex->desc.type != EXPORT_GLOBAL) {
                return false;
        }
        const struct globaltype *gt = module_globaltype(m, ex->desc.idx);
        return gt->mut == GLOBAL_CONST && gt->t == TYPE_i32;
}

void
dyld_init(struct dyld *d)
{
        memset(d, 0, sizeof(*d));
        LIST_HEAD_INIT(&d->objs);

        d->table_base = 1; /* do not use the first one */

        uint32_t stack_size = 16 * 1024; /* XXX TODO guess from main obj */
        d->memory_base = stack_size;
}

struct dyld_object *
dyld_find_object_by_instance(struct dyld *d, const struct instance *inst)
{
        struct dyld_object *obj;
        LIST_FOREACH(obj, &d->objs, q) {
                if (obj->instance == inst) {
                        return obj;
                }
        }
        return NULL;
}

struct dyld_object *
dyld_find_object_by_name(struct dyld *d, const struct name *name)
{
        struct dyld_object *obj;
        LIST_FOREACH(obj, &d->objs, q) {
                if (!compare_name(obj->name, name)) {
                        return obj;
                }
        }
        return NULL;
}

int
dyld_load_needed_objects(struct dyld *d)
{
        int ret = 0;
        struct dyld_object *obj;
        LIST_FOREACH(obj, &d->objs, q) {
                const struct dylink_needs *needs = &obj->module->dylink->needs;
                uint32_t i;
                for (i = 0; i < needs->count; i++) {
                        const struct name *name = &needs->names[i];
                        if (dyld_find_object_by_name(d, name)) {
                                continue;
                        }
                        char filename[PATH_MAX];
                        snprintf(filename, sizeof(filename), "%.*s",
                                 CSTR(name));
                        ret = dyld_load_object_from_file(d, name, filename);
                        if (ret != 0) {
                                goto fail;
                        }
                }
        }
fail:
        return ret;
}

int
dyld_allocate_local_import_object(struct dyld *d, struct dyld_object *obj)
{
        int ret;
        uint32_t nent = 0;

        nent += 2; /* env.__table_base, env.__memory_base */
        nent += obj->ngots;
        nent += obj->nplts;
        ret = import_object_alloc(nent, &obj->local_import_obj);
        if (ret != 0) {
                goto fail;
        }
fail:
        return ret;
}

int
dyld_create_got(struct dyld *d, struct dyld_object *obj)
{
        int ret;
        const struct module *m = obj->module;
        uint32_t ngots = 0;
        uint32_t nplts = 0;
        uint32_t i;
        for (i = 0; i < m->nimports; i++) {
                const struct import *im = &m->imports[i];
                if (is_GOT_mem_import(m, im)) {
                        ngots++;
                }
                if (is_GOT_func_import(m, im)) {
                        ngots++;
                }
                if (is_env_func_import(m, im)) {
                        nplts++;
                }
        }
        obj->nplts = nplts;
        obj->ngots = ngots;

        ret = dyld_allocate_local_import_object(d, obj);
        if (ret != 0) {
                goto fail;
        }
        obj->plts = calloc(nplts, sizeof(*obj->plts));
        obj->gots = calloc(ngots, sizeof(*obj->gots));
        if (obj->plts == NULL || obj->gots == NULL) {
                return ENOMEM;
        }

        struct import_object_entry *e;
        e = obj->local_import_obj->entries;

        obj->memory_base_global.type = &globaltype_i32_const;
        global_set_i32(&obj->memory_base_global, obj->memory_base);
        e->module_name = &name_env;
        e->name = &name_memory_base;
        e->type = IMPORT_GLOBAL;
        e->u.global = &obj->memory_base_global;
        e++;

        obj->table_base_global.type = &globaltype_i32_const;
        global_set_i32(&obj->table_base_global, obj->table_base);
        e->module_name = &name_env;
        e->name = &name_table_base;
        e->type = IMPORT_GLOBAL;
        e->u.global = &obj->table_base_global;
        e++;

        struct globalinst *got = obj->gots;
        struct dyld_plt *plt = obj->plts;

        for (i = 0; i < m->nimports; i++) {
                const struct import *im = &m->imports[i];
                if (is_GOT_mem_import(m, im) || is_GOT_func_import(m, im)) {
                        got->type = &globaltype_i32_mut;

                        e->module_name = &im->module_name;
                        e->name = &im->name;
                        e->type = IMPORT_GLOBAL;
                        e->u.global = got;

                        got++;
                        e++;
                }
                if (is_env_func_import(m, im)) {
                        plt->sym = &im->name;
                        plt->refobj = obj;
                        plt->dyld = d;

                        struct funcinst *fi = &plt->pltfi;
                        fi->is_host = true;
                        fi->u.host.instance = (void *)plt;
                        fi->u.host.type = &m->types[im->desc.u.typeidx];
                        fi->u.host.func = dyld_plt;

                        e->module_name = &im->module_name;
                        e->name = &im->name;
                        e->type = IMPORT_FUNC;
                        e->u.func = fi;

                        plt++;
                        e++;
                }
        }

        assert(got == obj->gots + ngots);
        assert(plt == obj->plts + nplts);
        assert(e == obj->local_import_obj->entries + nent);
        return 0;
fail:
        return ret;
}

int
dyld_allocate_memory(struct dyld *d, struct dyld_object *obj)
{
        const struct dylink_mem_info *minfo = &obj->module->dylink->mem_info;
        d->memory_base = align_up(d->memory_base, minfo->memoryalignment);
        obj->memory_base = d->memory_base;
        d->memory_base += minfo->memorysize;
        return 0;
}

int
dyld_allocate_table(struct dyld *d, struct dyld_object *obj)
{
        const struct dylink_mem_info *minfo = &obj->module->dylink->mem_info;
        d->table_base = align_up(d->table_base, minfo->tablealignment);
        obj->table_base = d->table_base;
        d->table_base += minfo->tablesize;

        /*
         * Note: the following logic likely allocates more than
         * what's actually necessary because:
         *
         * - not all exported functions are actually imported
         * - some of exported functions can refer to the same
         *   function instance.
         */
        const struct module *m = obj->module;
        uint32_t nexports = 0;
        uint32_t i;
        for (i = 0; i < m->nexports; i++) {
                if (is_func_export(m, &m->exports[i])) {
                        nexports++;
                }
        }
        obj->table_export_base = d->table_base;
        d->table_base += nexports;
        obj->nexports = nexports;
        return 0;
}

int
dyld_load_object_from_file(struct dyld *d, const struct name *name,
                           const char *filename)
{
        struct dyld_object *obj;
        int ret;
        obj = zalloc(sizeof(*obj));
        if (obj == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        obj->name = name;
        ret = map_file(filename, (void *)&obj->bin, &obj->binsz);
        if (ret != 0) {
                goto fail;
        }
        struct load_context lctx;
        load_context_init(&lctx);
        ret = module_create(&obj->module, obj->bin, obj->bin + obj->binsz,
                            &lctx);
        load_context_clear(&lctx);
        if (ret != 0) {
                goto fail;
        }
        if (obj->module->dylink == NULL) {
                xlog_error("module %.*s doesn't have dylink.0", CSTR(name));
                ret = EINVAL;
                goto fail;
        }
        ret = dyld_create_got(d, obj);
        if (ret != 0) {
                goto fail;
        }
        ret = dyld_allocate_memory(d, obj);
        if (ret != 0) {
                goto fail;
        }
        ret = dyld_allocate_table(d, obj);
        if (ret != 0) {
                goto fail;
        }
        LIST_INSERT_TAIL(&d->objs, obj, q);
        return 0;
fail:
        /* todo: destroy obj */
        return ret;
}

int
dyld_commit_allocation(struct dyld *d)
{
        int ret;

        struct tabletype *tt = &d->tt;
        tt->et = TYPE_FUNCREF;
        tt->lim.min = d->table_base;
        tt->lim.max = d->table_base;
        ret = table_instance_create(&d->tableinst, tt);
        if (ret != 0) {
                goto fail;
        }

        struct memtype *mt = &d->mt;
        d->memory_base = align_up(d->memory_base, WASM_PAGE_SIZE);
        mt->lim.min = d->memory_base / WASM_PAGE_SIZE;
        mt->lim.max = WASM_MAX_PAGES;
        mt->flags = 0;
        ret = memory_instance_create(&d->meminst, mt);
        if (ret != 0) {
                goto fail;
        }
fail:
        return ret;
}

uint32_t
dyld_register_funcinst(struct dyld *d, struct dyld_object *obj,
                       const struct funcinst *fi)
{
        struct tableinst *ti = d->tableinst;
        uint32_t i;
        for (i = obj->table_export_base; i < obj->nexports; i++) {
                struct val val;
                table_get(ti, i, &val);
                if (val.u.funcref.func == fi) {
                        return i;
                }
                if (val.u.funcref.func == NULL) {
                        val.u.funcref.func = fi;
                        table_set(ti, i, &val);
                        return i;
                }
        }
        assert(false);
}

int
dyld_resolve_symbol(struct dyld *d, struct dyld_object *refobj,
                    enum symtype symtype, const struct name *sym,
                    uint32_t *resultp)
{
        struct dyld_object *obj;
        enum exporttype etype;
        if (symtype == SYM_TYPE_FUNC) {
                etype = EXPORT_FUNC;
        } else {
                etype = EXPORT_GLOBAL;
        }
        LIST_FOREACH(obj, &d->objs, q) {
                const struct module *m = obj->module;
                uint32_t i;
                for (i = 0; i < m->nexports; i++) {
                        const struct export *ex = &m->exports[i];
                        const struct exportdesc *ed = &ex->desc;
                        if (etype != ed->type ||
                            compare_name(sym, &ex->name)) {
                                continue;
                        }
                        const struct instance *inst = obj->instance;
                        uint32_t addr;
                        if (symtype == SYM_TYPE_FUNC) {
                                /*
                                 * XXX should check the functype
                                 */
                                const struct funcinst *fi =
                                        VEC_ELEM(inst->funcs, ed->idx);
                                addr = dyld_register_funcinst(d, obj, fi);
                        } else {
                                struct globalinst *gi =
                                        VEC_ELEM(inst->globals, ed->idx);
                                if (!is_global_type_i32_const(gi->type)) {
                                        continue;
                                }
                                addr = global_get_i32(gi) + obj->memory_base;
                        }
                        *resultp = addr;
                        return 0;
                }
        }
        return ENOENT;
}

int
dyld_resolve_got_symbols(struct dyld *d, struct dyld_object *refobj)
{
        const struct module *m = refobj->module;
        struct globalinst *got = refobj->gots;
        int ret;
        uint32_t i;
        for (i = 0; i < m->nimports; i++) {
                const struct import *im = &m->imports[i];
                enum symtype symtype;
                if (is_GOT_mem_import(m, im)) {
                        symtype = SYM_TYPE_MEM;
                } else if (is_GOT_func_import(m, im)) {
                        symtype = SYM_TYPE_FUNC;
                } else {
                        continue;
                }
                uint32_t addr;
                ret = dyld_resolve_symbol(d, refobj, symtype, &im->name,
                                          &addr);
                if (ret != 0) {
                        goto fail;
                }
                global_set_i32(got, addr);
                got++;
        }
        assert(got == obj->gots + obj->ngots);
        return 0;
fail:
        return ret;
}

int
dyld_resolve_all_got_symbols(struct dyld *d)
{
        struct dyld_object *obj;
        LIST_FOREACH(obj, &d->objs, q) {
                int ret = dyld_resolve_got_symbols(d, obj);
                if (ret != 0) {
                        return ret;
                }
        }
        return 0;
}

int
dyld_load_main_object_from_file(struct dyld *d, const char *filename)
{
        int ret;
        /* REVISIT: name of main module */
        ret = dyld_load_object_from_file(d, NULL, filename);
        if (ret != 0) {
                goto fail;
        }
        ret = dyld_load_needed_objects(d);
        if (ret != 0) {
                goto fail;
        }
        ret = dyld_commit_allocation(d);
        if (ret != 0) {
                goto fail;
        }
        ret = dyld_resolve_all_got_symbols(d);
fail:
        return ret;
}

void
dyld_object_destroy(struct dyld_object *obj)
{
        free(obj->gots);
        free(obj->plts);
        if (obj->instance != NULL) {
                instance_destroy(obj->instance);
        }
        if (obj->module != NULL) {
                module_destroy(obj->module);
        }
        if (obj->bin != NULL) {
                unmap_file((void *)obj->bin, obj->binsz);
        }
        free(obj);
}

void
dyld_clear(struct dyld *d)
{
        struct dyld_object *obj;
        while ((obj = LIST_FIRST(&d->objs)) != 0) {
                LIST_REMOVE(&d->objs, obj, q);
                dyld_object_destroy(obj);
        }
        if (d->meminst != NULL) {
                memory_instance_destroy(d->meminst);
        }
        if (d->tableinst != NULL) {
                table_instance_destroy(d->tableinst);
        }
        memset(d, 0, sizeof(*d));
}
