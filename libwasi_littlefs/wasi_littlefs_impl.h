#include "lfs.h"

#include "wasi_impl.h"
#include "wasi_vfs_types.h"

struct wasi_fdinfo_littlefs {
        struct wasi_fdinfo_user user;
        enum {
                WASI_LFS_TYPE_NONE = 0,
                WASI_LFS_TYPE_FILE,
                WASI_LFS_TYPE_DIR,
        } type;
        union {
                lfs_file_t file;
                lfs_dir_t dir;
        } u;
};

struct wasi_vfs_littlefs {
        struct wasi_vfs vfs;
        lfs_t *lfs;
};

int lfs_error_to_errno(enum lfs_error lfs_error);

struct wasi_fdinfo_littlefs *
wasi_fdinfo_to_littlefs(struct wasi_fdinfo *fdinfo);
struct wasi_vfs_littlefs *wasi_vfs_to_littlefs(const struct wasi_vfs *vfs);
