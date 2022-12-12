struct wasi_threads_instance;
struct import_object;

void wasi_threads_instance_destroy(struct wasi_threads_instance *inst);
int wasi_threads_instance_create(struct wasi_threads_instance **resultp);

/*
 * wasi_threads_instance_set_thread_spawn_args: set wasi-threads parameters
 *
 * An embedder should call this before starting the execution of the
 * instance, which might uses wasi-threads.
 * Otherwise, `wasi:thread_spawn` would fail.
 *
 * Background:
 * `wasi:thread_spawn` is supposed to "instantiate the module again".
 * Unfortunately it isn't always obvious which module it is, especially
 * when multiple modules are involved. In this implementation, an embedder
 * needs to specify it explicitly via this api.
 * Also, this api takes the parameters for the re-instantiation.
 * (import_object here)
 *
 * cf. https://github.com/WebAssembly/wasi-threads/issues/13
 */
int wasi_threads_instance_set_thread_spawn_args(
        struct wasi_threads_instance *inst, struct module *m,
        const struct import_object *imports);

int import_object_create_for_wasi_threads(struct wasi_threads_instance *wasi,
                                          struct import_object **impp);
