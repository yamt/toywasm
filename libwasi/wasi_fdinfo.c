#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>

#include "wasi_vfs.h"
#include "wasi_impl.h"
#include "wasi_vfs_impl_host.h"

bool
wasi_fdinfo_is_prestat(const struct wasi_fdinfo *fdinfo)
{
        return fdinfo->type == WASI_FDINFO_PRESTAT;
}

bool
wasi_fdinfo_unused(struct wasi_fdinfo *fdinfo)
{
        return fdinfo->type == WASI_FDINFO_UNUSED;
}

const char *
wasi_fdinfo_path(struct wasi_fdinfo *fdinfo)
{
        switch (fdinfo->type) {
        case WASI_FDINFO_PRESTAT:
                return fdinfo->u.u_prestat.prestat_path;
        case WASI_FDINFO_USER:
                return fdinfo->u.u_user.path;
        case WASI_FDINFO_UNUSED:
                return NULL;
        }
        assert(false);
        return NULL;
}

const struct wasi_vfs *
wasi_fdinfo_vfs(const struct wasi_fdinfo *fdinfo)
{
        const struct wasi_vfs *vfs;
        switch (fdinfo->type) {
        case WASI_FDINFO_PRESTAT:
                vfs = &fdinfo->u.u_prestat.vfs;
                break;
        case WASI_FDINFO_USER:
                vfs = fdinfo->u.u_user.vfs;
                break;
        default:
                assert(false);
        }
        assert(vfs != NULL);
        assert(vfs->ops != NULL);
        return vfs;
}

struct wasi_fdinfo *
wasi_fdinfo_alloc(void)
{
        struct wasi_fdinfo *fdinfo = zalloc(sizeof(*fdinfo));
        if (fdinfo == NULL) {
                return NULL;
        }
        fdinfo->type = WASI_FDINFO_UNUSED;
        fdinfo->refcount = 0;
        fdinfo->blocking = 1;
        assert(wasi_fdinfo_unused(fdinfo));
        return fdinfo;
}

void
wasi_fdinfo_free(struct wasi_fdinfo *fdinfo)
{
        if (fdinfo == NULL) {
                return;
        }
        assert(fdinfo->refcount == 0);
        switch (fdinfo->type) {
        case WASI_FDINFO_PRESTAT:
                free(fdinfo->u.u_prestat.prestat_path);
                free(fdinfo->u.u_prestat.wasm_path);
                break;
        case WASI_FDINFO_USER:
                assert(fdinfo->u.u_user.hostfd == -1);
                assert(fdinfo->u.u_user.path == NULL);
                assert(fdinfo->u.u_user.dir == NULL);
                break;
        case WASI_FDINFO_UNUSED:
                break;
        }
        free(fdinfo);
}

void
wasi_fdinfo_release(struct wasi_instance *wasi, struct wasi_fdinfo *fdinfo)
{
        if (fdinfo == NULL) {
                return;
        }
        toywasm_mutex_lock(&wasi->lock);
        assert(fdinfo->refcount > 0);
        fdinfo->refcount--;
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        if (fdinfo->refcount == 1) {
                /* wake up drain */
                toywasm_cv_signal(&wasi->cv, &wasi->lock);
        }
#endif
        if (fdinfo->refcount == 0) {
                wasi_fdinfo_free(fdinfo);
        }
        toywasm_mutex_unlock(&wasi->lock);
}

int
wasi_fdinfo_close(struct wasi_fdinfo *fdinfo)
{
        int ret = 0;
        switch (fdinfo->type) {
        case WASI_FDINFO_PRESTAT:
                free(fdinfo->u.u_prestat.prestat_path);
                free(fdinfo->u.u_prestat.wasm_path);
                fdinfo->u.u_prestat.prestat_path = NULL;
                fdinfo->u.u_prestat.wasm_path = NULL;
                break;
        case WASI_FDINFO_USER:
                ret = wasi_vfs_fd_close(fdinfo);
                break;
        case WASI_FDINFO_UNUSED:
                break;
        }
        return ret;
}
