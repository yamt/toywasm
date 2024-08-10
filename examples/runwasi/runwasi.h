#include <stdint.h>

struct mem_context;
struct import_object;
struct module;
struct meminst;
struct tableinst;

int runwasi_module(
        struct mem_context *mctx, const struct module *m, unsigned int ndirs,
        char **dirs, unsigned int nenvs, const char *const *envs, int argc,
        const char *const *argv, const int stdio_fds[3],
        struct import_object *base_imports,
        void (*set_host_instance_resources)(void *hi_arg, struct meminst *mem,
                                            struct tableinst *func_table),
        void *hi_arg, uint32_t *wasi_exit_code_p);

int runwasi(struct mem_context *mctx, const char *filename, unsigned int ndirs,
            char **dirs, unsigned int nenvs, const char *const *envs, int argc,
            const char *const *argv, const int stdio_fds[3],
            struct import_object *base_imports,
            void (*set_host_instance_resources)(void *hi_arg,
                                                struct meminst *mem,
                                                struct tableinst *func_table),
            void *hi_arg, uint32_t *wasi_exit_code_p);
