struct wasi_instance;
struct import_object;

/*
 * a wasi_instance represents all data associated with a WASI "process",
 * including command line arguments, environment variables, and a file
 * descriptor table.
 * usually you should create a WASI instance for each WASI-using modules.
 *
 * Note: for now, the data for wasi-threads is maintained as a separate
 * object. see wasi_threads.h.
 */

int wasi_instance_create(struct wasi_instance **instp);
void wasi_instance_set_args(struct wasi_instance *inst, int argc,
                            char *const *argv);
void wasi_instance_set_environ(struct wasi_instance *inst, int nenvs,
                               char *const *envs);
void wasi_instance_destroy(struct wasi_instance *inst);

/*
 * wasi_instance_prestat_add_mapdir specifies a host directory to
 * expose to the wasi instance. the "path" argument is a string in
 * a format of "GUEST_DIR::HOST_DIR".
 * (the current api has an obvious flaw about names containing "::".)
 *
 * wasi_instance_prestat_add(i, "dir") is mostly an equivalent of
 * wasi_instance_prestat_add_mapdir("i, "dir::dir").
 */
int wasi_instance_prestat_add(struct wasi_instance *wasi, const char *path);
int wasi_instance_prestat_add_mapdir(struct wasi_instance *wasi,
                                     const char *path);

/*
 * wasi_instance_add_hostfd:
 *
 * add a wasi fd which maps to the host fd.
 *
 * wasi_instance_populate_stdio_with_hostfd is a shorthand of
 *    wasi_instance_add_hostfd(wasi, 0, 0);
 *    wasi_instance_add_hostfd(wasi, 1, 1);
 *    wasi_instance_add_hostfd(wasi, 2, 2);
 */
int wasi_instance_add_hostfd(struct wasi_instance *inst, uint32_t wasmfd,
                             int hostfd);
int wasi_instance_populate_stdio_with_hostfd(struct wasi_instance *inst);

/*
 * wasi_instance_exit_code:
 *
 * obtain the exit code from WASI proc_exit.
 *
 * it's available only when the execution of the module ended with
 * ETOYWASMTRAP + TRAP_VOLUNTARY_EXIT.
 */
uint32_t wasi_instance_exit_code(struct wasi_instance *wasi);

/*
 * import_object_create_for_wasi:
 *
 * create an import_object which contains all external values exported
 * from the wasi instance. that is, host functions like
 * "wasi_snapshot_preview1:path_open".
 */
int import_object_create_for_wasi(struct wasi_instance *wasi,
                                  struct import_object **impp);
