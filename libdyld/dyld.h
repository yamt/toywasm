#include <stdint.h>

#include "host_instance.h"
#include "platform.h"
#include "slist.h"
#include "toywasm_config.h"
#include "type.h"
#include "vec.h"

struct dyld_object;
struct mem_context;

struct dyld_options {
        struct import_object *base_import_obj;

        /*
         * stack size.
         * something similar to wasm-ld's "-z stack-size=xxx" option.
         *
         * Note: this option is used only when the main module is PIE.
         * otherwise, the stack in the main module is just used in the
         * same way as non dynamic-linking modules.
         */
        uint32_t stack_size;

        /*
         * library search path.
         * something similar to LD_LIBRARY_PATH.
         */
        unsigned int npaths;
        const char *const *paths;

        /*
         * disable lazy plt binding.
         * something similar to LD_BIND_NOW.
         */
        bool bindnow;

#if defined(TOYWASM_ENABLE_DYLD_DLFCN)
        bool enable_dlfcn;
#endif
};

struct dyld {
        struct import_object *shared_import_obj;

        uint32_t memory_base;
        uint32_t table_base;

        bool pie; /* the main module is pie */

        struct meminst *meminst;
        struct tableinst *tableinst;
        struct globalinst *stack_pointer;
        struct globalinst heap_base;
        struct globalinst heap_end;
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        struct functype *c_longjmp_ft;
        struct taginst c_longjmp;
        struct taginst cpp_exception;
#endif

        union {
                struct {
                        struct globalinst *heap_base;
                        struct globalinst *heap_end;
                } nonpie;
                struct {
                        struct memtype mt;
                        struct tabletype tt;
                        struct globalinst stack_pointer;
                } pie;
        } u;

        SLIST_HEAD(struct dyld_object) objs;

        struct dyld_options opts;

#if defined(TOYWASM_ENABLE_DYLD_DLFCN)
        VEC(, struct dyld_dynamic_object) dynobjs;
#endif

        struct mem_context *mctx;
};

__BEGIN_EXTERN_C

struct import_object;
struct mem_context;
struct meminst;
struct tableinst;

void dyld_init(struct dyld *d, struct mem_context *mctx);
void dyld_clear(struct dyld *d);
int dyld_load(struct dyld *d, const char *filename);
int dyld_execute_init_funcs(struct dyld *d);
struct instance *dyld_main_object_instance(struct dyld *d);
struct meminst *dyld_memory(struct dyld *d);
struct tableinst *dyld_func_table(struct dyld *d);
void dyld_options_set_defaults(struct dyld_options *opts);
int import_object_create_for_dyld(struct mem_context *mctx, struct dyld *d,
                                  struct import_object **impp);
void dyld_print_stats(struct dyld *d);

__END_EXTERN_C
