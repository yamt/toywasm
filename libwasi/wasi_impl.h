#include <stdbool.h>
#include <stdint.h>

#include "host_instance.h"
#include "lock.h"
#include "wasi_abi.h"
#include "xlog.h"

enum wasi_fdinfo_type {
        WASI_FDINFO_PRESTAT,
        WASI_FDINFO_USER, /* vfs-based file */
};

struct wasi_fdinfo {
        enum wasi_fdinfo_type type;
        uint32_t refcount;
};

struct wasi_fdinfo_prestat {
        struct wasi_fdinfo fdinfo;
        char *prestat_path;
        char *wasm_path; /* NULL means same as prestat_path */
        const struct wasi_vfs *vfs;
};

struct wasi_fdinfo_user {
        struct wasi_fdinfo fdinfo;
        const struct wasi_vfs *vfs;
        char *path;
        uint32_t blocking;
};

struct wasi_fdinfo_host {
        struct wasi_fdinfo_user user;
        int hostfd;
        void *dir; /* DIR * */
};

struct wasi_table {
        uint32_t reserved_slots;
        VEC(, struct wasi_fdinfo *) table;
};

enum wasi_table_idx {
        WASI_TABLE_FILES = 0,
        WASI_NTABLES = 1,
};

struct wasi_instance {
        struct host_instance hi;

        TOYWASM_MUTEX_DEFINE(lock);
        TOYWASM_CV_DEFINE(cv);
        struct wasi_table fdtable[WASI_NTABLES] GUARDED_VAR(
                lock); /* indexed by wasi fd */

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

/* fdinfo */
bool wasi_fdinfo_is_prestat(const struct wasi_fdinfo *fdinfo);
const char *wasi_fdinfo_path(struct wasi_fdinfo *fdinfo);
const struct wasi_vfs *wasi_fdinfo_vfs(struct wasi_fdinfo *fdinfo);
void wasi_fdinfo_init(struct wasi_fdinfo *fdinfo);
void wasi_fdinfo_user_init(struct wasi_fdinfo_user *fdinfo_user);
void wasi_fdinfo_clear(struct wasi_fdinfo *fdinfo);
struct wasi_fdinfo *wasi_fdinfo_alloc_prestat(void);
struct wasi_fdinfo_prestat *wasi_fdinfo_to_prestat(struct wasi_fdinfo *fdinfo);
struct wasi_fdinfo_user *wasi_fdinfo_to_user(struct wasi_fdinfo *fdinfo);
void wasi_fdinfo_free(struct wasi_fdinfo *fdinfo);
void wasi_fdinfo_release(struct wasi_instance *wasi,
                         struct wasi_fdinfo *fdinfo);
int wasi_fdinfo_close(struct wasi_fdinfo *fdinfo);

/* fdtable */
int wasi_fd_lookup(struct wasi_instance *wasi, uint32_t wasifd,
                   struct wasi_fdinfo **infop);
int wasi_hostfd_lookup(struct wasi_instance *wasi, uint32_t wasifd,
                       int *hostfdp, struct wasi_fdinfo **fdinfop);
int wasi_userfd_lookup(struct wasi_instance *wasi, uint32_t wasifd,
                       struct wasi_fdinfo **fdinfop);
int wasi_hostfd_add(struct wasi_instance *wasi, int hostfd, char *path,
                    uint16_t fdflags, uint32_t *wasifdp);

/* table */
void wasi_table_affix(struct wasi_instance *wasi, enum wasi_table_idx idx,
                      uint32_t wasifd, struct wasi_fdinfo *fdinfo)
        REQUIRES(wasi->lock);
int wasi_table_lookup_locked(struct wasi_instance *wasi,
                             enum wasi_table_idx idx, uint32_t wasifd,
                             struct wasi_fdinfo **infop) REQUIRES(wasi->lock);
int wasi_table_lookup(struct wasi_instance *wasi, enum wasi_table_idx idx,
                      uint32_t wasifd, struct wasi_fdinfo **infop);
int wasi_table_lookup_locked_for_close(struct exec_context *ctx,
                                       struct wasi_instance *wasi,
                                       enum wasi_table_idx idx,
                                       uint32_t wasifd,
                                       struct wasi_fdinfo **fdinfop, int *retp)
        REQUIRES(wasi->lock);
int wasi_table_expand(struct wasi_instance *wasi, enum wasi_table_idx idx,
                      uint32_t maxfd) REQUIRES(wasi->lock);
void wasi_table_clear(struct wasi_instance *wasi, enum wasi_table_idx idx);
int wasi_table_alloc_slot(struct wasi_instance *wasi, enum wasi_table_idx idx,
                          uint32_t *wasifdp) REQUIRES(wasi->lock);
int wasi_table_fdinfo_add(struct wasi_instance *wasi, enum wasi_table_idx idx,
                          struct wasi_fdinfo *fdinfo, uint32_t *wasifdp);
struct wasi_fdinfo **wasi_table_slot_ptr(struct wasi_instance *wasi,
                                         enum wasi_table_idx idx,
                                         uint32_t wasifd) REQUIRES(wasi->lock);
