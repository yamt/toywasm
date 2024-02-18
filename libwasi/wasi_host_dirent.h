#include <stdbool.h>
#include <stdint.h>

struct wasi_dirent;

int wasi_host_dir_close(void *dir);
/*
 * Note about directory offset:
 * offset 0 should be the start of the directory.
 * other offset values are implementation defined.
 */
int wasi_host_dir_seek(void *dir, uint64_t offset);
int wasi_host_dir_read(void *dir, struct wasi_dirent *wde,
                       const uint8_t **namep, bool *eod);
