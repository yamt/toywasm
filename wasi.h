struct wasi_instance;
struct import_object;
int wasi_instance_create(struct wasi_instance **resultp);
void wasi_instance_set_args(struct wasi_instance *inst, int argc,
                            char *const *argv);
void wasi_instance_destroy(struct wasi_instance *inst);
int wasi_instance_prestat_add(struct wasi_instance *inst, const char *path);
int import_object_create_for_wasi(struct wasi_instance *wasi,
                                  struct import_object **impp);
