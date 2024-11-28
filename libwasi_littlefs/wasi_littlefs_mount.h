#include <stdint.h>

struct wasi_vfs;
struct wasi_littlefs_mount_cfg;

int wasi_littlefs_mount_file(const char *path,
                             const struct wasi_littlefs_mount_cfg *cfg,
                             struct wasi_vfs **vfsp);
