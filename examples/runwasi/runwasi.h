#include <stdint.h>

int runwasi(const char *filename, unsigned int ndirs, char **dirs,
            unsigned int nenvs, char **envs, int argc, char **argv,
            const int stdio_fds[3], uint32_t *wasi_exit_code_p);
