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

int
wasi_instance_prestat_add_mapdir_littlefs(struct wasi_instance *wasi,
                                          const char *path)
{
        struct wasi_vfs *vfs = NULL;
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

        ret = wasi_littlefs_mount_file(image_path, &vfs);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_instance_prestat_add_vfs(wasi, mapdir_string, vfs, true);
        if (ret != 0) {
                goto fail;
        }
        free(image_path);
        return 0;
fail:
        if (vfs != NULL) {
                wasi_littlefs_umount_file(vfs);
        }
        free(image_path);
        return ret;
}
