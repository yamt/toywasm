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

#include "nbio.h"
#include "util.h"
#include "wasi_littlefs_impl.h"
#include "wasi_littlefs_mount.h"
#include "wasi_vfs_impl_littlefs.h"
#include "xlog.h"

static int
wasi_lfs_bd_read(const struct lfs_config *cfg, lfs_block_t block,
                 lfs_off_t off, void *buffer, lfs_size_t size)
{
        struct wasi_vfs_lfs *lfs_vfs = cfg->context;
        int fd = lfs_vfs->fd;
        off_t offset = block * cfg->block_size + off;
        LFS_STAT_INC(lfs_vfs->stat.bd_read);
        LFS_STAT_ADD(lfs_vfs->stat.bd_read_bytes, size);
        ssize_t ssz = pread(fd, buffer, size, offset);
        if (ssz == -1) {
                return LFS_ERR_IO;
        }
        return 0;
}

static int
wasi_lfs_bd_prog(const struct lfs_config *cfg, lfs_block_t block,
                 lfs_off_t off, const void *buffer, lfs_size_t size)
{
        struct wasi_vfs_lfs *lfs_vfs = cfg->context;
        int fd = lfs_vfs->fd;
        off_t offset = block * cfg->block_size + off;
        LFS_STAT_INC(lfs_vfs->stat.bd_prog);
        LFS_STAT_ADD(lfs_vfs->stat.bd_prog_bytes, size);
        ssize_t ssz = pwrite(fd, buffer, size, offset);
        if (ssz == -1) {
                return LFS_ERR_IO;
        }
        return 0;
}

static int
wasi_lfs_bd_erase(const struct lfs_config *cfg, lfs_block_t block)
{
#if defined(TOYWASM_ENABLE_LITTLEFS_STATS)
        struct wasi_vfs_lfs *lfs_vfs = cfg->context;
#endif
        LFS_STAT_INC(lfs_vfs->stat.bd_erase);
        return 0;
}

static int
wasi_lfs_bd_sync(const struct lfs_config *cfg)
{
        struct wasi_vfs_lfs *lfs_vfs = cfg->context;
        int fd = lfs_vfs->fd;
        LFS_STAT_INC(lfs_vfs->stat.bd_sync);
        int ret = fsync(fd);
        if (ret != 0) {
                return LFS_ERR_IO;
        }
        return 0;
}

#define MUTEX(cfg) (&((struct wasi_vfs_lfs *)(cfg)->context)->lock)

static int
wasi_lfs_fs_lock(const struct lfs_config *cfg) ACQUIRES(MUTEX(cfg))
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
wasi_lfs_fs_unlock(const struct lfs_config *cfg) RELEASES(MUTEX(cfg))
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

        /*
         * Note: lfs requires users to specify the correct block_size.
         * ideally, this should be given by cli option / api params.
         * alternatively, it's probably possible to auto-detect the block
         * size by scanning the filesystem image.
         * for now, we simply hardcode a value.
         */
        lfs_size_t block_size = 4096;

        /*
         * Note: read/prog sizes themselves do not affect the filesystem
         * structure. there are some indirect relationships though.
         * for example, cache_size, which restricts inline_max, should be
         * a multiple of read/prog size.
         */
        lfs_size_t read_size = 256;
        lfs_size_t prog_size = 256;
        lfs_size_t cache_size = 256;
        assert((cache_size % read_size) == 0);
        assert((cache_size % prog_size) == 0);
        assert((block_size % cache_size) == 0);

        /*
         * calculate block_count from the file size.
         * alternatively, we can use block_size = 0 to tell littlefs
         * to trust the value in the superblock.
         */
        struct stat st;
        ret = fstat(vfs_lfs->fd, &st);
        if (ret != 0) {
                ret = errno;
                assert(ret > 0);
                goto fail;
        }
        if ((st.st_size % block_size) != 0) {
                ret = EINVAL;
                goto fail;
        }
        lfs_size_t block_count = st.st_size / block_size;
        assert(block_count * block_size == st.st_size);

        struct lfs_config *lfs_config = &vfs_lfs->lfs_config;
        lfs_config->context = vfs_lfs;
        lfs_config->read = wasi_lfs_bd_read;
        lfs_config->prog = wasi_lfs_bd_prog;
        lfs_config->erase = wasi_lfs_bd_erase;
        lfs_config->sync = wasi_lfs_bd_sync;
        lfs_config->lock = wasi_lfs_fs_lock;
        lfs_config->unlock = wasi_lfs_fs_unlock;
        lfs_config->read_size = read_size;
        lfs_config->prog_size = prog_size;
        lfs_config->block_size = block_size;
        lfs_config->block_count = block_count;

        /*
         * disable block-level wear-leveling because there is little point
         * to perform it on a filesystem image.
         */
        lfs_config->block_cycles = -1;

        lfs_config->cache_size = cache_size;

        /*
         * arbitrary chosen.
         * can cover (8 * lookahead_size) blocks.
         */
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

#if defined(TOYWASM_ENABLE_LITTLEFS_STATS)
#define LFS_PRINT_STAT(st, item)                                              \
        nbio_printf("%s = %" PRIu64 "\n", #item, (st)->item)
#else
#define LFS_PRINT_STAT(st, item)                                              \
        do {                                                                  \
        } while (0)
#endif

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
        LFS_PRINT_STAT(&vfs_lfs->stat, bd_read);
        LFS_PRINT_STAT(&vfs_lfs->stat, bd_read_bytes);
        LFS_PRINT_STAT(&vfs_lfs->stat, bd_prog);
        LFS_PRINT_STAT(&vfs_lfs->stat, bd_prog_bytes);
        LFS_PRINT_STAT(&vfs_lfs->stat, bd_erase);
        LFS_PRINT_STAT(&vfs_lfs->stat, bd_sync);
        free(vfs_lfs);
        return 0;
}
