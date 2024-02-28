struct wasi_instance;

int wasi_instance_prestat_add_mapdir_littlefs(struct wasi_instance *wasi,
                                              const char *path);
int wasi_littlefs_mount_file(const char *path, struct wasi_vfs **vfsp);
int wasi_littlefs_umount(struct wasi_vfs *vfs);
