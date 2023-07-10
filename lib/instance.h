#include <stdint.h>

struct module;
struct instance;
struct exec_context;
struct resulttype;
struct val;
struct import_object;
struct report;
struct name;

/*
 * This API is inspired from js-api.
 * https://webassembly.github.io/spec/js-api/index.html#instances
 */
int instance_create(const struct module *m, struct instance **instp,
                    const struct import_object *imports,
                    struct report *report);

/*
 * Instead of instance_create, you can use instance_create_no_init and
 * then instance_create_execute_init. That way you might be able to handle
 * some kind of errors more flexibly. These functions are introduced
 * to deal with some of corner cases you can see in opam-2.0.0 linking.wast.
 * cf. https://github.com/WebAssembly/spec/issues/1530
 */
int instance_create_no_init(const struct module *m, struct instance **instp,
                            const struct import_object *imports,
                            struct report *report);
int instance_create_execute_init(struct instance *inst,
                                 struct exec_context *ctx);

/*
 * Note: If you have multiple instances linked together
 * with import/export, usually the only safe way to destroy those
 * instances is to destroy them together at once after ensuring none of
 * them are running. Note that import/export is not necessarily a one-way
 * dependency. Because funcref values, which are implemented as bare host
 * pointers in this engine, can be freely passed among instances, linked
 * instances can have references each other.
 * cf. https://github.com/WebAssembly/spec/issues/1513
 */
void instance_destroy(struct instance *inst);

/*
 * instance_execute_func:
 *
 * execute the function.
 *
 * in addition to usual <errno.h> error numbers, this function can
 * return following toywasm-specific errors.
 * see the comment on ETOYWASMxxx macros in exec_context.h.
 */
int instance_execute_func(struct exec_context *ctx, uint32_t funcidx,
                          const struct resulttype *paramtype,
                          const struct resulttype *resulttype);

/*
 * instance_execute_func_nocheck is meant to be used where the caller
 * already knows the function type for sure.
 */
int instance_execute_func_nocheck(struct exec_context *ctx, uint32_t funcidx);

/*
 * instance_execute_continue:
 *
 * when one of instance_execute_xxx functions including the following ones
 * returned a restartable error, the app can resume the execution by calling
 * this function.
 *
 *   - instance_execute_func
 *   - instance_execute_func_nocheck
 *   - instance_execute_continue
 *
 * see also: instance_execute_handle_restart
 */
int instance_execute_continue(struct exec_context *ctx);

/*
 * instance_execute_handle_restart:
 *
 * an equivalent of keep calling instance_execute_handle_restart_once
 * until it returns a success or a non-restartable error.
 */
int instance_execute_handle_restart(struct exec_context *ctx, int exec_ret);

/*
 * instance_execute_handle_restart_once:
 *
 * after performing the default handling of a restartable error,
 * continue the execution as instance_execute_continue does.
 *
 * Note: this function can return a restartable error.
 *
 * Note: if the given exec_ret is not a restartable error, this function
 * just returns it as it is.
 */
int instance_execute_handle_restart_once(struct exec_context *ctx,
                                         int exec_ret);

/*
 * see the comment in type.h about the concept of import_object.
 */
int import_object_create_for_exports(struct instance *inst,
                                     const struct name *module_name,
                                     struct import_object **resultp);
void import_object_destroy(struct import_object *im);
int import_object_alloc(uint32_t nentries, struct import_object **resultp);

struct meminst;
struct memtype;
int memory_instance_create(struct meminst **mip, const struct memtype *mt);
void memory_instance_destroy(struct meminst *mi);

/*
 * create_satisfying_shared_memories:
 *
 * create shared memories to satisfy all shared memory imports
 * in the module.
 *
 * mainly for wasi-threads, where it's host's responsibility to
 * provide the linear memory.
 */
int create_satisfying_shared_memories(const struct module *module,
                                      struct import_object **imop);

void instance_print_stats(const struct instance *inst);
