#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>

#include "wasi_host_subr.h"
#include "wasi_impl.h"
#include "wasi_vfs.h"
#include "wasi_vfs_impl_host.h"

bool
wasi_fdinfo_is_prestat(const struct wasi_fdinfo *fdinfo)
{
        return fdinfo->type == WASI_FDINFO_PRESTAT;
}

const char *
wasi_fdinfo_path(struct wasi_fdinfo *fdinfo)
{
        switch (fdinfo->type) {
        case WASI_FDINFO_PRESTAT:
                return wasi_fdinfo_to_prestat(fdinfo)->prestat_path;
        case WASI_FDINFO_USER:
                return wasi_fdinfo_to_user(fdinfo)->path;
        }
        assert(false);
        return NULL;
}

struct wasi_vfs *
wasi_fdinfo_vfs(struct wasi_fdinfo *fdinfo)
{
        struct wasi_vfs *vfs;
        switch (fdinfo->type) {
        case WASI_FDINFO_PRESTAT:
                vfs = wasi_fdinfo_to_prestat(fdinfo)->vfs;
                break;
        case WASI_FDINFO_USER:
                vfs = wasi_fdinfo_to_user(fdinfo)->vfs;
                break;
        }
        assert(vfs != NULL);
        assert(vfs->ops != NULL);
        return vfs;
}

void
wasi_fdinfo_init(struct wasi_fdinfo *fdinfo)
{
        fdinfo->refcount = 0;
}

void
wasi_fdinfo_user_init(struct wasi_fdinfo_user *fdinfo_user)
{
        wasi_fdinfo_init(&fdinfo_user->fdinfo);
        fdinfo_user->fdinfo.type = WASI_FDINFO_USER;
        fdinfo_user->blocking = 1;
        fdinfo_user->path = NULL;
}

int
wasi_fdinfo_alloc_prestat(struct wasi_fdinfo **fdinfop)
{
        struct wasi_fdinfo_prestat *fdinfo_prestat =
                zalloc(sizeof(*fdinfo_prestat));
        if (fdinfo_prestat == NULL) {
                return ENOMEM;
        }
        struct wasi_fdinfo *fdinfo = &fdinfo_prestat->fdinfo;
        wasi_fdinfo_init(fdinfo);
        fdinfo->type = WASI_FDINFO_PRESTAT;
        *fdinfop = fdinfo;
        /* Note: the caller should initialize fdinfo_prestat->vfs */
        return 0;
}

struct wasi_fdinfo_prestat *
wasi_fdinfo_to_prestat(struct wasi_fdinfo *fdinfo)
{
        assert(fdinfo->type == WASI_FDINFO_PRESTAT);
        struct wasi_fdinfo_prestat *fdinfo_prestat = (void *)fdinfo;
        assert(&fdinfo_prestat->fdinfo == fdinfo);
        return fdinfo_prestat;
}

struct wasi_fdinfo_user *
wasi_fdinfo_to_user(struct wasi_fdinfo *fdinfo)
{
        assert(fdinfo->type == WASI_FDINFO_USER);
        struct wasi_fdinfo_user *fdinfo_user = (void *)fdinfo;
        assert(&fdinfo_user->fdinfo == fdinfo);
        return fdinfo_user;
}

void
wasi_fdinfo_clear(struct wasi_fdinfo *fdinfo)
{
        struct wasi_fdinfo_prestat *fdinfo_prestat;
        assert(fdinfo->refcount == 0);
        switch (fdinfo->type) {
        case WASI_FDINFO_PRESTAT:
                fdinfo_prestat = wasi_fdinfo_to_prestat(fdinfo);
                free(fdinfo_prestat->prestat_path);
                free(fdinfo_prestat->wasm_path);
                break;
        case WASI_FDINFO_USER:
                assert(wasi_fdinfo_to_user(fdinfo)->path == NULL);
                if (wasi_fdinfo_is_host(fdinfo)) {
                        assert(wasi_fdinfo_to_host(fdinfo)->hostfd == -1);
                        assert(wasi_fdinfo_to_host(fdinfo)->dir == NULL);
                }
                break;
        }
}

void
wasi_fdinfo_free(struct wasi_fdinfo *fdinfo)
{
        if (fdinfo == NULL) {
                return;
        }
        wasi_fdinfo_clear(fdinfo);
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
        struct wasi_fdinfo_prestat *fdinfo_prestat;
        struct wasi_fdinfo_user *fdinfo_user;
        int ret = 0;
        switch (fdinfo->type) {
        case WASI_FDINFO_PRESTAT:
                fdinfo_prestat = wasi_fdinfo_to_prestat(fdinfo);
                free(fdinfo_prestat->prestat_path);
                free(fdinfo_prestat->wasm_path);
                fdinfo_prestat->prestat_path = NULL;
                fdinfo_prestat->wasm_path = NULL;
                break;
        case WASI_FDINFO_USER:
                fdinfo_user = wasi_fdinfo_to_user(fdinfo);
                ret = wasi_vfs_fd_close(fdinfo);
                free(fdinfo_user->path);
                fdinfo_user->path = NULL;
                break;
        }
        return ret;
}
