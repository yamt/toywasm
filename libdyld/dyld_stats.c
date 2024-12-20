#include "dyld.h"
#include "dyld_impl.h"
#include "escape.h"
#include "mem.h"
#include "nbio.h"

void
dyld_print_stats(struct dyld *d)
{
        struct dyld_object *obj;
        SLIST_FOREACH(obj, &d->objs, q) {
                struct escaped_string e;
                escape_name(&e, obj->name);

                nbio_printf("%12.*s"
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
                            " mod %10zu"
#if defined(TOYWASM_ENABLE_HEAP_TRACKING_PEAK)
                            " (peak %10zu)"
#endif
#endif
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
                            " inst %10zu"
#if defined(TOYWASM_ENABLE_HEAP_TRACKING_PEAK)
                            " (peak %10zu)"
#endif
#endif
                            "\n",
                            ECSTR(&e)
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
                                    ,
                            obj->module_mctx.allocated
#if defined(TOYWASM_ENABLE_HEAP_TRACKING_PEAK)
                            ,
                            obj->module_mctx.peak
#endif
#endif
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
                            ,
                            obj->instance_mctx.allocated
#if defined(TOYWASM_ENABLE_HEAP_TRACKING_PEAK)
                            ,
                            obj->instance_mctx.peak
#endif
#endif
                );
                escaped_string_clear(&e);
        }
}
