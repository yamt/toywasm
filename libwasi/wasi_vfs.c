#include <errno.h>

#include "wasi_impl.h"
#include "wasi_path_subr.h"
#include "wasi_vfs.h"
#include "wasi_vfs_ops.h"
#include "wasi_vfs_types.h"

static const struct wasi_vfs_ops *
fdinfo_vfs_ops(const struct wasi_fdinfo *fdinfo)
{
        const struct wasi_vfs *vfs = fdinfo->u.u_user.vfs;
        return vfs->ops;
}

static const struct wasi_vfs_ops *
path_vfs_ops(const struct path_info *pi)
{
        return fdinfo_vfs_ops(pi->dirfdinfo);
}

static bool
check_xdev(const struct path_info *pi1, const struct path_info *pi2)
{
        return path_vfs_ops(pi1) != path_vfs_ops(pi2);
}

#include "wasi_vfs_dispatch.h"
