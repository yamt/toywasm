#if !defined(_TOYWASM_LIBWASI_WASI_VFS_TYPES_H_)
#define _TOYWASM_LIBWASI_WASI_VFS_TYPES_H_
#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>

struct path_open_params {
        uint32_t lookupflags;
        uint32_t wasmoflags;
        uint64_t rights_base;
        uint32_t fdflags;
};

typedef uint64_t wasi_off_t;

/* (ab)used bits from preview1 abi (wasi_abi.h) */
struct wasi_fdinfo;
struct wasi_filestat;
struct wasi_dirent;

/* wasi_utimes.h */
struct utimes_args;

/* wasi_path_subr.h */
struct path_info;

/* sys/uio.h */
struct iovec;

struct wasi_vfs {
        const struct wasi_vfs_ops *ops;
};
#endif /* !defined(_TOYWASM_LIBWASI_WASI_VFS_TYPES_H_) */
