#include <stdint.h>

int runwasi(const char *filename, unsigned int ndirs, char **dirs,
            unsigned int nenvs, const char *const *envs, int argc,
            const char *const *argv, const int stdio_fds[3],
            uint32_t *wasi_exit_code_p);
