#include <stdbool.h>

struct wasi_fdinfo;
struct wasi_vfs;

const struct wasi_vfs *wasi_get_vfs_littlefs();
int wasi_fdinfo_alloc_littlefs(struct wasi_fdinfo **fdinfop);
bool wasi_fdinfo_is_littlefs(struct wasi_fdinfo *fdinfo);
