#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#define _NETBSD_SOURCE

#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lfs.h"

#include "util.h"
#include "wasi_littlefs_impl.h"
#include "wasi_littlefs_mount.h"
#include "wasi_vfs_impl_littlefs.h"
#include "xlog.h"

static int
wasi_lfs_read(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off,
              void *buffer, lfs_size_t size)
{
        struct wasi_vfs_lfs *lfs_vfs = cfg->context;
        int fd = lfs_vfs->fd;
        off_t offset = block * cfg->block_size + off;
        ssize_t ssz = pread(fd, buffer, size, offset);
        if (ssz == -1) {
                return LFS_ERR_IO;
        }
        return 0;
}

static int
wasi_lfs_prog(const struct lfs_config *cfg, lfs_block_t block, lfs_off_t off,
              const void *buffer, lfs_size_t size)
{
        struct wasi_vfs_lfs *lfs_vfs = cfg->context;
        int fd = lfs_vfs->fd;
        off_t offset = block * cfg->block_size + off;
        ssize_t ssz = pwrite(fd, buffer, size, offset);
        if (ssz == -1) {
                return LFS_ERR_IO;
        }
        return 0;
}

static int
wasi_lfs_erase(const struct lfs_config *cfg, lfs_block_t block)
{
        return 0;
}

static int
wasi_lfs_sync(const struct lfs_config *cfg)
{
        struct wasi_vfs_lfs *lfs_vfs = cfg->context;
        int fd = lfs_vfs->fd;
        int ret = fsync(fd);
        if (ret != 0) {
                return LFS_ERR_IO;
        }
        return 0;
}

#define MUTEX(cfg) (&((struct wasi_vfs_lfs *)(cfg)->context)->lock)

static int
wasi_lfs_lock(const struct lfs_config *cfg) ACQUIRES(MUTEX(cfg))
{
        /*
         * REVISIT: toywasm_mutex_lock is not really appropriate because
         * toywasm_mutex_lock is a no-op unless TOYWASM_ENABLE_WASM_THREADS
         * is enabled. consider the cases where a mounted filesystem
         * (wasi_vfs_lfs) is shared among single-threaded instances.
         */
        toywasm_mutex_lock(MUTEX(cfg));
        return 0;
}

static int
wasi_lfs_unlock(const struct lfs_config *cfg) RELEASES(MUTEX(cfg))
{
        toywasm_mutex_unlock(MUTEX(cfg));
        return 0;
}

int
wasi_littlefs_mount_file(const char *path, struct wasi_vfs **vfsp)
{
        struct wasi_vfs_lfs *vfs_lfs = NULL;
        int ret;

        vfs_lfs = zalloc(sizeof(*vfs_lfs));
        if (vfs_lfs == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        toywasm_mutex_init(&vfs_lfs->lock);
        vfs_lfs->vfs.ops = wasi_get_lfs_vfs_ops();
        vfs_lfs->fd = open(path, O_RDWR);
        if (vfs_lfs->fd == -1) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        struct stat st;
        ret = fstat(vfs_lfs->fd, &st);
        if (ret != 0) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        lfs_size_t sector_size = 256;
        lfs_size_t block_size = 4096;
        if ((st.st_size % block_size) != 0) {
                ret = EINVAL;
                goto fail;
        }
        lfs_size_t block_count = st.st_size / block_size;
        assert(block_count * block_size == st.st_size);
        struct lfs_config *lfs_config = &vfs_lfs->lfs_config;
        lfs_config->context = vfs_lfs;
        lfs_config->read = wasi_lfs_read;
        lfs_config->prog = wasi_lfs_prog;
        lfs_config->erase = wasi_lfs_erase;
        lfs_config->sync = wasi_lfs_sync;
        lfs_config->lock = wasi_lfs_lock;
        lfs_config->unlock = wasi_lfs_unlock;
        lfs_config->read_size = sector_size;
        lfs_config->prog_size = sector_size;
        lfs_config->block_size = block_size;
        lfs_config->block_count = block_count;
        lfs_config->block_cycles = -1;
        lfs_config->cache_size = sector_size;
        lfs_config->lookahead_size = 8;
        ret = lfs_mount(&vfs_lfs->lfs, lfs_config);
        if (ret != 0) {
                xlog_error("lfs_mount failed with %d", ret);
                ret = lfs_error_to_errno(ret);
                goto fail;
        }
        *vfsp = &vfs_lfs->vfs;
        return 0;
fail:
        if (vfs_lfs != NULL) {
                if (vfs_lfs->fd != -1) {
                        close(vfs_lfs->fd);
                }
                toywasm_mutex_destroy(&vfs_lfs->lock);
                free(vfs_lfs);
        }
        return ret;
}

int
wasi_littlefs_umount_file(struct wasi_vfs *vfs)
{
        struct wasi_vfs_lfs *vfs_lfs = wasi_vfs_to_lfs(vfs);
        assert(vfs_lfs->fd != -1);
        int ret = lfs_unmount(&vfs_lfs->lfs);
        if (ret != 0) {
                xlog_trace("lfs_unmount failed with %d", ret);
                return lfs_error_to_errno(ret);
        }
        ret = close(vfs_lfs->fd);
        if (ret != 0) {
                /* log and ignore. */
                xlog_error("ignoring close failure %d", ret);
        }
        toywasm_mutex_destroy(&vfs_lfs->lock);
        free(vfs_lfs);
        return 0;
}
