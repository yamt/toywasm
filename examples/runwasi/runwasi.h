#include <stdint.h>

struct mem_context;
struct import_object;

int runwasi(struct mem_context *mctx, const char *filename, unsigned int ndirs,
            char **dirs, unsigned int nenvs, const char *const *envs, int argc,
            const char *const *argv, const int stdio_fds[3],
            struct import_object *base_imports, uint32_t *wasi_exit_code_p);
