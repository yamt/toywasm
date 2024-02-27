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
#include "wasi_littlefs.h"
#include "wasi_littlefs_impl.h"
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

int
wasi_instance_prestat_add_mapdir_littlefs(struct wasi_instance *wasi,
                                          const char *path)
{
        struct wasi_vfs_lfs *vfs_lfs = NULL;
        char *image_path = NULL;
        const char *mapdir_string;
        int ret;

        /* IMAGE_FILE::HOST_DIR::MAP_DIR */
        const char *colon = strchr(path, ':');
        if (colon == NULL || colon[1] != ':') {
                ret = EINVAL;
                goto fail;
        }
        image_path = strndup(path, colon - path);
        if (image_path == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        mapdir_string = colon + 2;

        vfs_lfs = zalloc(sizeof(*vfs_lfs));
        if (vfs_lfs == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        vfs_lfs->vfs.ops = wasi_get_lfs_vfs_ops();
        vfs_lfs->fd = open(image_path, O_RDWR);
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
        ret = wasi_instance_prestat_add_vfs(wasi, mapdir_string, &vfs_lfs->vfs,
                                            true);
        if (ret != 0) {
                goto fail;
        }
        free(image_path);
        return 0;
fail:
        if (vfs_lfs != NULL) {
                close(vfs_lfs->fd);
        }
        free(vfs_lfs);
        free(image_path);
        return ret;
}
