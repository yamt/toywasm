#include <stdbool.h>

struct wasi_fdinfo;
struct wasi_vfs;

struct wasi_vfs *wasi_get_vfs_host(void);
int wasi_fdinfo_alloc_host(struct wasi_fdinfo **fdinfop);
bool wasi_fdinfo_is_host(struct wasi_fdinfo *fdinfo);
