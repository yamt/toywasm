#include <stdbool.h>
#include <stdint.h>

struct wasi_dirent;

int wasi_host_dir_close(void *dir);
int wasi_host_dir_rewind(void *dir);
int wasi_host_dir_seek(void *dir, uint64_t offset);
int wasi_host_dir_read(void *dir, struct wasi_dirent *wde,
                       const uint8_t **namep, bool *eod);
