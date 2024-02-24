struct wasi_fdinfo;

void wasi_vfs_impl_host_init_file(struct wasi_fdinfo *fdinfo);
void wasi_vfs_impl_host_init_prestat(struct wasi_fdinfo *fdinfo);

int wasi_vfs_impl_host_fdinfo_alloc(struct wasi_fdinfo **fdinfop);
