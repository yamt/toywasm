struct wasi_instance;

int wasi_instance_prestat_add_littlefs(struct wasi_instance *wasi,
                                       const char *path);
