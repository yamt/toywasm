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

/*
 * wasi_threads_instance_join: wait for completion of all threads
 * spawned by wasi:thread_spawn in the wasi-threads instance.
 */
void wasi_threads_instance_join(struct wasi_threads_instance *inst);

const uint32_t *
wasi_threads_interrupt_pointer(struct wasi_threads_instance *inst);

struct trap_info;
void wasi_threads_propagate_trap(struct wasi_threads_instance *wasi,
                                 const struct trap_info *trap);
const struct trap_info *
wasi_threads_instance_get_trap(struct wasi_threads_instance *wasi);

int import_object_create_for_wasi_threads(struct wasi_threads_instance *wasi,
                                          struct import_object **impp);
