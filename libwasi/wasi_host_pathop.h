#include <stdint.h>

struct path_info {
        char *hostpath;
        char *wasmpath;
};

struct path_open_params {
        uint32_t lookupflags;
        uint32_t wasmoflags;
        uint64_t rights_base;
        uint32_t fdflags;
};

struct wasi_filestat;
struct utimes_args;

int wasi_host_path_open(const struct path_info *pi,
                        const struct path_open_params *params, int *fdp);
int wasi_host_path_unlink(const struct path_info *pi);
int wasi_host_path_mkdir(const struct path_info *pi);
int wasi_host_path_rmdir(const struct path_info *pi);
int wasi_host_path_symlink(const char *target_buf, const struct path_info *pi);
int wasi_host_path_readlink(const struct path_info *pi, char *buf,
                            size_t buflen, size_t *resultp);
int wasi_host_path_link(const struct path_info *pi1,
                        const struct path_info *pi2);
int wasi_host_path_rename(const struct path_info *pi1,
                          const struct path_info *pi2);
int wasi_host_path_stat(const struct path_info *pi, struct wasi_filestat *stp);
int wasi_host_path_lstat(const struct path_info *pi,
                         struct wasi_filestat *stp);
int wasi_host_path_utimes(const struct path_info *pi,
                          const struct utimes_args *args);
int wasi_host_path_lutimes(const struct path_info *pi,
                           const struct utimes_args *args);
