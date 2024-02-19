#include <stdbool.h>
#include <stdint.h>

struct wasi_fdinfo;
struct wasi_dirent;

int wasi_host_dir_close(struct wasi_fdinfo *fdinfo);
int wasi_host_dir_rewind(struct wasi_fdinfo *fdinfo);
int wasi_host_dir_seek(struct wasi_fdinfo *fdinfo, uint64_t offset);
int wasi_host_dir_read(struct wasi_fdinfo *fdinfo, struct wasi_dirent *wde,
                       const uint8_t **namep, bool *eod);
