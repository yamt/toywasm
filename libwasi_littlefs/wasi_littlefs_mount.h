struct wasi_vfs;

int wasi_littlefs_mount_file(const char *path, struct wasi_vfs **vfsp);
int wasi_littlefs_umount_file(struct wasi_vfs *vfs);
