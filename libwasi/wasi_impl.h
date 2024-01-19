#include <dirent.h> /* DIR */
#include <stdbool.h>
#include <stdint.h>

#include "host_instance.h"
#include "lock.h"
#include "wasi_abi.h"
#include "xlog.h"

enum wasi_fdinfo_type {
        WASI_FDINFO_UNUSED,
        WASI_FDINFO_PRESTAT,
        WASI_FDINFO_USER,
};

struct wasi_fdinfo {
        enum wasi_fdinfo_type type;
        union {
                /* WASI_FDINFO_PRESTAT */
                struct {
                        char *prestat_path;
                        char *wasm_path; /* NULL means same as prestat_path */
                } u_prestat;
                /* WASI_FDINFO_USER */
                struct {
                        int hostfd;
                        DIR *dir;
                        char *path;
                } u_user;
        } u;
        uint32_t refcount;
        uint32_t blocking;
};

struct wasi_instance {
        struct host_instance hi;

        TOYWASM_MUTEX_DEFINE(lock);
        TOYWASM_CV_DEFINE(cv);
        VEC(, struct wasi_fdinfo *)
        fdtable GUARDED_VAR(lock); /* indexed by wasi fd */

        int argc;
        const char *const *argv;
        int nenvs;
        const char *const *envs;

        uint32_t exit_code;
};

#define WASI_HOST_FUNC(NAME, TYPE)                                            \
        {                                                                     \
                .name = NAME_FROM_CSTR_LITERAL(#NAME), .type = TYPE,          \
                .func = wasi_##NAME,                                          \
        }

#define WASI_HOST_FUNC2(NAME, FUNC, TYPE)                                     \
        {                                                                     \
                .name = NAME_FROM_CSTR_LITERAL(NAME), .type = TYPE,           \
                .func = FUNC,                                                 \
        }

#if defined(TOYWASM_ENABLE_TRACING)
#define WASI_TRACE                                                            \
        do {                                                                  \
                xlog_trace("WASI: %s called", __func__);                      \
                host_func_dump_params(ft, params);                            \
        } while (0)
#else
#define WASI_TRACE                                                            \
        do {                                                                  \
        } while (0)
#endif

uint32_t wasi_convert_errno(int host_errno);

#define wasi_copyout(c, h, w, l, a) host_func_copyout(c, h, w, l, a)
#define wasi_copyin(c, h, w, l, a) host_func_copyin(c, h, w, l, a)

struct exec_context;
bool wasi_fdinfo_is_prestat(const struct wasi_fdinfo *fdinfo);
bool wasi_fdinfo_unused(struct wasi_fdinfo *fdinfo);
const char *wasi_fdinfo_path(struct wasi_fdinfo *fdinfo);
struct wasi_fdinfo *wasi_fdinfo_alloc(void);
void wasi_fd_affix(struct wasi_instance *wasi, uint32_t wasifd,
                   struct wasi_fdinfo *fdinfo) REQUIRES(wasi->lock);
int wasi_fd_lookup_locked(struct wasi_instance *wasi, uint32_t wasifd,
                          struct wasi_fdinfo **infop) REQUIRES(wasi->lock);
int wasi_fd_lookup(struct wasi_instance *wasi, uint32_t wasifd,
                   struct wasi_fdinfo **infop);
void wasi_fdinfo_release(struct wasi_instance *wasi,
                         struct wasi_fdinfo *fdinfo);
int wasi_fdinfo_close(struct wasi_fdinfo *fdinfo);
int wasi_fd_lookup_locked_for_close(struct exec_context *ctx,
                                    struct wasi_instance *wasi,
                                    uint32_t wasifd,
                                    struct wasi_fdinfo **fdinfop, int *retp)
        REQUIRES(wasi->lock);
int wasi_hostfd_lookup(struct wasi_instance *wasi, uint32_t wasifd,
                       int *hostfdp, struct wasi_fdinfo **fdinfop);
int wasi_userfd_lookup(struct wasi_instance *wasi, uint32_t wasifd,
                       struct wasi_fdinfo **fdinfop);
int wasi_fdtable_expand(struct wasi_instance *wasi, uint32_t maxfd)
        REQUIRES(wasi->lock);
void wasi_fdtable_free(struct wasi_instance *wasi);
int wasi_fd_alloc(struct wasi_instance *wasi, uint32_t *wasifdp)
        REQUIRES(wasi->lock);
int wasi_fd_add(struct wasi_instance *wasi, int hostfd, char *path,
                uint16_t fdflags, uint32_t *wasifdp);
