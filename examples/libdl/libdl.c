/*
 * a simple dlopen/dlsym implementation backed by toywasm dyld_dlfcn
 */

#include <stddef.h>
#include <string.h>

#include "dlfcn.h"

__attribute__((import_module("dyld")))
__attribute__((import_name("load_object"))) int
dyld_load_module(const char *name, size_t namelen, int flags, void *handlep);

__attribute__((import_module("dyld")))
__attribute__((import_name("resolve_symbol"))) int
dyld_resolve_symbol(void *handle, int symtype, const char *name,
                    size_t namelen, void **addrp);

#define DYLD_SYMBOL_TYPE_FUNC 1
#define DYLD_SYMBOL_TYPE_MEMORY 2

void *
dlopen(const char *name, int mode)
{
        int ret;
        void *h;
        /* mode is ignored for now */
        ret = dyld_load_module(name, strlen(name), 0, &h);
        if (ret != 0) {
                return NULL;
        }
        return h;
}

void *
dlsym(void *h, const char *name)
{
        int ret;
        void *vp;
        ret = dyld_resolve_symbol(h, DYLD_SYMBOL_TYPE_FUNC, name, strlen(name),
                                  &vp);
        if (ret != 0) {
                ret = dyld_resolve_symbol(h, DYLD_SYMBOL_TYPE_MEMORY, name,
                                          strlen(name), &vp);
        }
        if (ret != 0) {
                return NULL;
        }
        return vp;
}

const char *
dlerror()
{
        return "error";
}

void
dlclose(void *handle)
{
        /*
         * nothing for now.
         *
         * a real unloading is difficult without garbage collector
         * as linked modules can have circular dependencies via funcrefs.
         * probably should mark the object "unloaded" at least?
         */
}
