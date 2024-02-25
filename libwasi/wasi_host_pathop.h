#include <stdint.h>

struct path_open_params;
struct wasi_fdinfo;
struct wasi_filestat;
struct utimes_args;
struct path_info;

int wasi_host_path_fdinfo_alloc(struct path_info *pi,
                                struct wasi_fdinfo **fdinfop);
int wasi_host_path_open(struct path_info *pi,
                        const struct path_open_params *params,
                        struct wasi_fdinfo *fdinfo);
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
