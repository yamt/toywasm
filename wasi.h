struct wasi_instance;
struct import_object;
int wasi_instance_create(struct wasi_instance **resultp);
void wasi_instance_destroy(struct wasi_instance *inst);
int import_object_create_for_wasi(struct wasi_instance *wasi,
                                  struct import_object **impp);
