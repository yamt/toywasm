struct wasi_instance;
struct wasi_vfs;

int wasi_instance_prestat_add_mapdir_littlefs(struct wasi_instance *wasi,
                                              const char *path);
int wasi_littlefs_mount_file(const char *path, struct wasi_vfs **vfsp);
int wasi_littlefs_umount_file(struct wasi_vfs *vfs);
