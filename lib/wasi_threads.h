struct wasi_threads_instance;
struct import_object;

void wasi_threads_instance_destroy(struct wasi_threads_instance *inst);
int wasi_threads_instance_create(struct wasi_threads_instance **resultp);

int wasi_threads_instance_set_thread_spawn_args(
        struct wasi_threads_instance *inst, struct module *m,
        const struct import_object *imports);

int import_object_create_for_wasi_threads(struct wasi_threads_instance *wasi,
                                          struct import_object **impp);
