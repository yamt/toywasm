#if !defined(_TOYWASM_LIBWASI_LITTLEFS_WASI_LITTLEFS_H)
#define _TOYWASM_LIBWASI_LITTLEFS_WASI_LITTLEFS_H

#include "platform.h"

__BEGIN_EXTERN_C

struct wasi_instance;
struct wasi_vfs;

struct wasi_littlefs_mount_cfg {
        uint32_t disk_version;
        uint32_t block_size;
};

int wasi_instance_prestat_add_littlefs(
        struct wasi_instance *wasi, const char *path,
        const struct wasi_littlefs_mount_cfg *cfg, struct wasi_vfs **vfsp);

__END_EXTERN_C

#endif /* !defined(_TOYWASM_LIBWASI_LITTLEFS_WASI_LITTLEFS_H) */
