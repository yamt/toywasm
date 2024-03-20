#include "lfs_namespace.h"

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
        TOYWASM_MUTEX_DEFINE(lock);
        lfs_t lfs;
        struct lfs_config lfs_config;
        int fd;
#if defined(TOYWASM_ENABLE_LITTLEFS_STATS)
        struct {
                uint64_t bd_read;
                uint64_t bd_read_bytes;
                uint64_t bd_prog;
                uint64_t bd_prog_bytes;
                uint64_t bd_erase;
                uint64_t bd_sync;
        } stat;
#endif
};

#if defined(TOYWASM_ENABLE_LITTLEFS_STATS)
#define LFS_STAT_INC(st) (st)++
#define LFS_STAT_ADD(st, x) (st) += (x)
#else
#define LFS_STAT_INC(st)                                                      \
        do {                                                                  \
        } while (0)
#define LFS_STAT_ADD(st, x)                                                   \
        do {                                                                  \
        } while (0)
#endif

int lfs_error_to_errno(enum lfs_error lfs_error);

struct wasi_fdinfo_lfs *wasi_fdinfo_to_lfs(struct wasi_fdinfo *fdinfo);
struct wasi_vfs_lfs *wasi_vfs_to_lfs(struct wasi_vfs *vfs);
int wasi_littlefs_umount_file(struct wasi_vfs *vfs);
