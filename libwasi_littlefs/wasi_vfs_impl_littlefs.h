#include <stdbool.h>

struct wasi_fdinfo;
struct wasi_vfs;
struct wasi_vfs_ops;

const struct wasi_vfs_ops *wasi_get_lfs_vfs_ops(void);
int wasi_fdinfo_alloc_lfs(struct wasi_fdinfo **fdinfop, struct wasi_vfs *vfs);
bool wasi_fdinfo_is_lfs(struct wasi_fdinfo *fdinfo);
