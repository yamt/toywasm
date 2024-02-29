struct wasi_instance;
struct wasi_vfs;

int wasi_instance_prestat_add_mapdir_littlefs(struct wasi_instance *wasi,
                                              const char *path,
                                              struct wasi_vfs **vfsp);
