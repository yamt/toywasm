/*
 * simple host functions to allow dlopen/dlsym.
 *
 * Note: real module unloading is not possible w/o garbage collector
 * because linked wasm modules can easily form circlular dependencies
 * with funcrefs.
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "dyld.h"
#include "dyld_dlfcn_abi.h"
#include "dyld_impl.h"
#include "endian.h"
#include "exec.h"
#include "host_instance.h"
#include "mem.h"
#include "xlog.h"

static int
dyld_dynamic_load_object_by_name(struct dyld *d, const struct name *name,
                                 bool bindnow, struct dyld_object **objp)
{
        struct dyld_object *obj = NULL;
        int ret;
        ret = dyld_search_and_load_object_from_file(d, name, &obj);
        if (ret != 0) {
                goto fail;
        }
        ret = dyld_resolve_dependencies(d, obj, bindnow);
        if (ret != 0) {
                goto fail;
        }
        ret = dyld_execute_all_init_funcs(d, obj);
        if (ret != 0) {
                goto fail;
        }
        *objp = obj;
        return 0;
fail:
        if (obj != NULL) {
                /* TODO: implement cleanup */
        }
        return ret;
}

/*
 * CAVEAT: "name" here is just a name for dyld, not a path in
 * WASI filesystem.
 * just because i'm in a mood to keep wasi and dyld independent
 * each other for now.
 */
static int
dyld_dlfcn_load_object(struct exec_context *ctx, struct host_instance *hi,
                       const struct functype *ft, const struct cell *params,
                       struct cell *results)
{
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t namep = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t namelen = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t mode = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 3, i32);
        struct dyld *d = (void *)hi;
        int host_ret = 0;
        uint32_t user_ret; /* app-level error. just 0/1 for now. */
        int ret;

        if (mode != 0) { /* not used yet */
                user_ret = 1;
                goto fail;
        }

        void *vp;
        ret = host_func_memory_getptr(ctx, 0, namep, 0, namelen, &vp);
        if (ret != 0) {
                user_ret = 1;
                goto fail;
        }

        uint32_t idx = d->dynobjs.lsize;
        ret = VEC_PREALLOC(d->mctx, d->dynobjs, 1);
        if (ret != 0) {
                xlog_error("%s: dynobjs prealloc failed", __func__);
                user_ret = 1;
                goto fail;
        }
        struct dyld_dynamic_object *dobj = &VEC_ELEM(d->dynobjs, idx);
        memset(dobj, 0, sizeof(*dobj));

        char *name_data = mem_alloc(d->mctx, namelen + 1);
        if (name_data == NULL) {
                xlog_error("%s: malloc failed", __func__);
                user_ret = 1;
                goto fail;
        }
        memcpy(name_data, vp, namelen);
        name_data[namelen] = 0;

        struct name *name = &dobj->name;
        name->data = name_data;
        name->nbytes = namelen;
        ret = dyld_dynamic_load_object_by_name(d, name, false, &dobj->obj);
        if (ret != 0) {
                xlog_error("%s: dyld_dynamic_load_object_by_name failed",
                           __func__);
                mem_free(d->mctx, name_data, namelen + 1);
                user_ret = 1;
                goto fail;
        }
        d->dynobjs.lsize++;
        uint32_t handle = idx + 1;
        xlog_trace(
                "dyld: dyld:load_object succeeded for %.*s, handle %" PRIu32,
                CSTR(name), handle);
        uint32_t handle_le = host_to_le32(handle);
        host_ret =
                host_func_copyout(ctx, &handle_le, retp, sizeof(handle_le), 4);
        user_ret = 0;
fail:
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32, user_ret);
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static int
dyld_dlfcn_resolve_symbol(struct exec_context *ctx, struct host_instance *hi,
                          const struct functype *ft, const struct cell *params,
                          struct cell *results)
{
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t handle = HOST_FUNC_PARAM(ft, params, 0, i32);
        uint32_t symtype = HOST_FUNC_PARAM(ft, params, 1, i32);
        uint32_t namep = HOST_FUNC_PARAM(ft, params, 2, i32);
        uint32_t namelen = HOST_FUNC_PARAM(ft, params, 3, i32);
        uint32_t retp = HOST_FUNC_PARAM(ft, params, 4, i32);
        struct dyld *d = (void *)hi;
        int host_ret = 0;
        uint32_t user_ret; /* app-level error. just 0/1 for now. */
        int ret;

        enum symtype type;
        switch (symtype) {
        case DYLD_SYMBOL_TYPE_FUNC:
                type = SYM_TYPE_FUNC;
                break;
        case DYLD_SYMBOL_TYPE_MEMORY:
                type = SYM_TYPE_MEM;
                break;
        default:
                user_ret = 1;
                goto fail;
        }

        if (handle == 0) {
                user_ret = 1;
                goto fail;
        }
        uint32_t idx = handle - 1;
        if (idx >= d->dynobjs.lsize) {
                user_ret = 1;
                goto fail;
        }
        const struct dyld_dynamic_object *dobj = &VEC_ELEM(d->dynobjs, idx);

        void *vp;
        ret = host_func_memory_getptr(ctx, 0, namep, 0, namelen, &vp);
        if (ret != 0) {
                user_ret = 1;
                goto fail;
        }
        struct name name;
        name.data = vp;
        name.nbytes = namelen;

        /*
         * Note: this implementation only searches the symbols in
         * the object itself.
         * Typical dlsym implementions look at symbols in dependency
         * libraries as well.
         */
        uint32_t addr;
        ret = dyld_resolve_symbol_in_obj(LIST_FIRST(&d->objs), dobj->obj, type,
                                         &name, &addr);
        if (ret != 0) {
                xlog_trace("dyld: dyld:resolve_symbol dyld_resolve_symbol "
                           "failed for %.*s with %d",
                           CSTR(&name), ret);
                user_ret = 1;
                goto fail;
        }
        xlog_trace(
                "dyld: dyld:resolve_symbol succeeded for %.*s, addr %" PRIu32,
                CSTR(&name), addr);
        uint32_t addr_le = host_to_le32(addr);
        host_ret = host_func_copyout(ctx, &addr_le, retp, sizeof(addr_le), 4);
        user_ret = 0;
fail:
        if (host_ret == 0) {
                HOST_FUNC_RESULT_SET(ft, results, 0, i32, user_ret);
        }
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return host_ret;
}

static const struct host_func dyld_funcs[] = {
        HOST_FUNC_PREFIX(dyld_dlfcn_, load_object, "(iiii)i"),
        HOST_FUNC_PREFIX(dyld_dlfcn_, resolve_symbol, "(iiiii)i"),
};

static const struct name name_dyld = NAME_FROM_CSTR_LITERAL("dyld");

static const struct host_module module_dyld[] = {
        {
                .module_name = &name_dyld,
                .funcs = dyld_funcs,
                .nfuncs = ARRAYCOUNT(dyld_funcs),
        },
};

int
import_object_create_for_dyld(struct mem_context *mctx, struct dyld *d,
                              struct import_object **impp)
{
        return import_object_create_for_host_funcs(
                mctx, module_dyld, ARRAYCOUNT(module_dyld), (void *)d, impp);
}
