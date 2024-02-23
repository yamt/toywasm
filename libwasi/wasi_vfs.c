#include <errno.h>

#include "wasi_vfs_types.h"

#include "wasi_impl.h"
#include "wasi_path_subr.h"
#include "wasi_vfs.h"
#include "wasi_vfs_ops.h"

static const struct wasi_vfs_ops *
fdinfo_vfs_ops(const struct wasi_fdinfo *fdinfo)
{
        return wasi_fdinfo_vfs(fdinfo)->ops;
}

static const struct wasi_vfs *
path_vfs(const struct path_info *pi)
{
        return wasi_fdinfo_vfs(pi->dirfdinfo);
}

static const struct wasi_vfs_ops *
path_vfs_ops(const struct path_info *pi)
{
        return path_vfs(pi)->ops;
}

static bool
check_xdev(const struct path_info *pi1, const struct path_info *pi2)
{
        return path_vfs(pi1) != path_vfs(pi2);
}

#include "wasi_vfs_dispatch.h"
