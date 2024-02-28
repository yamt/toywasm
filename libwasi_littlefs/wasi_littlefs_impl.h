#include "lfs.h"

#include "wasi_impl.h"
#include "wasi_vfs_types.h"

struct wasi_fdinfo_lfs {
        struct wasi_fdinfo_user user;
        enum {
                WASI_LFS_TYPE_NONE = 0,
                WASI_LFS_TYPE_FILE,
                WASI_LFS_TYPE_DIR,
        } type;
        union {
                lfs_file_t file;
                struct {
                        lfs_dir_t dir;
                        struct lfs_info info;
                } dir;
        } u;
};

struct wasi_vfs_lfs {
        struct wasi_vfs vfs;
        lfs_t lfs;
        struct lfs_config lfs_config;
        int fd;
};

int lfs_error_to_errno(enum lfs_error lfs_error);

struct wasi_fdinfo_lfs *wasi_fdinfo_to_lfs(struct wasi_fdinfo *fdinfo);
struct wasi_vfs_lfs *wasi_vfs_to_lfs(const struct wasi_vfs *vfs);
