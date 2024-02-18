struct wasi_fdinfo;
struct wasi_filestet;
struct wasi_unstable_filestat;

int wasi_unstable_convert_filestat(const struct wasi_filestat *wst,
                                   struct wasi_unstable_filestat *uwst);
int wasi_userfd_reject_directory(struct wasi_fdinfo *fdinfo);
