#include <stdbool.h>

#include "toywasm_config.h"
#if defined(TOYWASM_ENABLE_DYLD)
#include "dyld.h"
#endif
#include "options.h"
#include "type.h"

struct repl_options {
        const char *prompt;
        struct repl_state *state;
        bool print_stats;
        bool allow_unresolved_functions;
#if defined(TOYWASM_ENABLE_DYLD)
        bool enable_dyld;
        struct dyld_options dyld_options;
#endif
        struct load_options load_options;
        struct exec_options exec_options;
};

struct repl_module_state {
        char *name;
        uint8_t *buf;
        size_t bufsize;
        bool buf_mapped;
        struct module *module;
        struct instance *inst;
#if defined(TOYWASM_ENABLE_WASI_THREADS)
        struct import_object *extra_import;
#endif
        struct import_object *unresolved_functions_import;
        struct mem_context *module_mctx;
        struct mem_context *instance_mctx;
};

struct repl_module_state_u {
        union {
#if defined(TOYWASM_ENABLE_DYLD)
                struct dyld dyld;
#endif
                struct repl_module_state repl;
        } u;
};

/* eg. const.wast has 366 modules */
#define MAX_MODULES 500

struct registered_name {
        struct name name;
        struct registered_name *next;
};

struct repl_state {
        VEC(, struct repl_module_state_u) modules;
        struct import_object *imports;
        unsigned int nregister;
        struct registered_name *registered_names;
        VEC(, struct val) param;
        VEC(, struct val) result;
        struct wasi_instance *wasi;
        struct wasi_threads_instance *wasi_threads;
        VEC(, struct wasi_vfs *) vfses;
        struct repl_options opts;
        struct timespec abstimeout;
        bool has_timeout;
        struct mem_context *mctx;
        struct mem_context *modules_mctx;
        struct mem_context *instances_mctx;
        struct mem_context *wasi_mctx;
        struct mem_context *dyld_mctx;
        struct mem_context *impobj_mctx;
};

void toywasm_repl_state_init(struct repl_state *state);
int toywasm_repl_set_timeout(struct repl_state *state, int timeout_ms);
int toywasm_repl(struct repl_state *state);

void toywasm_repl_reset(struct repl_state *state);
int toywasm_repl_load(struct repl_state *state, const char *modname,
                      const char *filename, bool trap_ok);
int toywasm_repl_register(struct repl_state *state, const char *modname,
                          const char *register_name);
int toywasm_repl_invoke(struct repl_state *state, const char *modname,
                        const char *cmd, uint32_t *exitcodep,
                        bool print_result);
void toywasm_repl_print_version(void);

int toywasm_repl_load_wasi(struct repl_state *state);
int toywasm_repl_set_wasi_args(struct repl_state *state, int argc,
                               const char *const *argv);
int toywasm_repl_set_wasi_environ(struct repl_state *state, int nenvs,
                                  const char *const *envs);
int toywasm_repl_set_wasi_prestat(struct repl_state *state, const char *path);
int toywasm_repl_set_wasi_prestat_littlefs(struct repl_state *state,
                                           const char *path);
