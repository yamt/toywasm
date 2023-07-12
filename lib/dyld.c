#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "dyld.h"
#include "dylink_type.h"
#include "fileio.h"
#include "load_context.h"
#include "module.h"
#include "type.h"
#include "util.h"
#include "xlog.h"

void
dyld_init(struct dyld *d)
{
        memset(d, 0, sizeof(*d));
}

int
dyld_load_needed_objects(struct dyld *d, struct dyld_object *obj)
{
        struct dylink_needs *needs = &obj->module->dylink->needs;
        uint32_t i;
        int ret = 0;
        for (i = 0; i < needs->count; i++) {
                const struct name *name = &needs->names[i];
                char filename[PATH_MAX];
                snprintf(filename, sizeof(filename), "%.*s", CSTR(name));
                ret = dyld_load_object_from_file(d, filename);
                if (ret != 0) {
                        goto fail;
                }
        }
fail:
        return ret;
}

int dyld_load_object_from_file(struct dyld *d, const char *name);

int
dyld_load_object_from_file(struct dyld *d, const char *name)
{
        struct dyld_object *obj;
        int ret;
        obj = zalloc(sizeof(*obj));
        if (obj == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        ret = map_file(name, (void *)&obj->bin, &obj->binsz);
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
                xlog_error("module %s doesn't have dylink.0", name);
                ret = EINVAL;
                goto fail;
        }
        ret = dyld_load_needed_objects(d, obj);
fail:
        return ret;
}
