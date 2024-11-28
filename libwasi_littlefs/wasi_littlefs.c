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

#include "util.h"
#include "wasi_littlefs.h"
#include "wasi_littlefs_impl.h"
#include "wasi_littlefs_mount.h"
#include "wasi_vfs_impl_littlefs.h"
#include "xlog.h"

int
wasi_instance_prestat_add_littlefs(struct wasi_instance *wasi,
                                   const char *path,
                                   const struct wasi_littlefs_mount_cfg *cfg,
                                   struct wasi_vfs **vfsp)
{
        struct wasi_vfs *vfs = NULL;
        char *image_path = NULL;
        const char *mapdir_string;
        int ret;

        /* IMAGE_FILE::HOST_DIR[::GUEST_DIR] */
        const char *coloncolon = strstr(path, "::");
        if (coloncolon == NULL) {
                ret = EINVAL;
                goto fail;
        }
        image_path = strndup(path, coloncolon - path);
        if (image_path == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        mapdir_string = coloncolon + 2;
        ret = wasi_littlefs_mount_file(image_path, cfg, &vfs);
        free(image_path);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_instance_prestat_add_vfs(wasi, mapdir_string, vfs);
        if (ret != 0) {
                goto fail;
        }
        *vfsp = vfs;
        return 0;
fail:
        if (vfs != NULL) {
                wasi_littlefs_umount_file(vfs);
        }
        return ret;
}
