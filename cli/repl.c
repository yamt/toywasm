/*
 * Note: the main purpose of this repl implementation is to run
 * (our fork of) the wasm3 testsuite:
 * https://github.com/yamt/wasm3/blob/toywasm-test/test/run-spec-test.py
 *
 * eg.
 * ./run-spec-test.py --exec ".../main_bin --repl --repl-prompt wasm3"
 */

#define _GNU_SOURCE      /* strdup */
#define _DARWIN_C_SOURCE /* strdup */
#define _NETBSD_SOURCE   /* strdup */

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cconv.h"
#include "endian.h"
#include "exec_context.h"
#include "exec_debug.h"
#include "fileio.h"
#include "instance.h"
#include "load_context.h"
#include "mem.h"
#include "module.h"
#include "module_writer.h"
#include "nbio.h"
#include "repl.h"
#include "report.h"
#include "str_to_uint.h"
#include "suspend.h"
#include "timeutil.h"
#include "toywasm_version.h"
#include "type.h"
#include "usched.h"
#if defined(TOYWASM_ENABLE_WASI)
#include "wasi.h"
#include "wasi_vfs.h"
#endif
#if defined(TOYWASM_ENABLE_WASI_THREADS)
#include "wasi_threads.h"
#endif
#if defined(TOYWASM_ENABLE_WASI_LITTLEFS)
#include "wasi_littlefs.h"
#endif
#include "xlog.h"

/*
 * Note: ref_is_null.wast distinguishes "ref.extern 0" and "ref.null extern"
 * while this implementation uses 0/NULL to represent "ref.null extern".
 *
 * wast                our representation
 * -----------------   -------------------
 * "ref.extern 0"      EXTERNREF_0
 * "ref.null extern"   NULL
 *
 * cf.
 * https://webassembly.github.io/spec/core/syntax/types.html#reference-types
 * > The type externref denotes the infinite union of all references to
 * > objects owned by the embedder and that can be passed into WebAssembly
 * > under this type.
 */
#define EXTERNREF_0 ((uintptr_t)(-1))

int
str_to_ptr(const char *s, int base, uintmax_t *resultp)
{
        if (!strcmp(s, "null")) {
                *resultp = 0;
                return 0;
        }
        int ret;
        ret = str_to_uint(s, base, resultp);
        if (ret != 0) {
                return ret;
        }
        if (*resultp == 0) {
                *resultp = EXTERNREF_0;
        }
        return 0;
}

/* read something like: "aabbcc\n" */
int
read_hex_from_stdin(uint8_t *p, size_t left)
{
        char buf[3];
        size_t sz;
        while (left > 0) {
                sz = fread(buf, 2, 1, stdin);
                if (sz == 0) {
                        return EIO;
                }
                buf[2] = 0;
                uintmax_t v;
#if defined(__GNUC__) && !defined(__clang__)
                v = 0;
#endif
                int ret = str_to_uint(buf, 16, &v);
                if (ret != 0) {
                        return ret;
                }
                *p++ = (uint8_t)v;
                left--;
        }
        sz = fread(buf, 1, 1, stdin);
        if (sz == 0) {
                return EIO;
        }
        if (buf[0] != '\n') {
                return EPROTO;
        }
        return 0;
}

static void
repl_unload(struct repl_state *state, struct repl_module_state *mod)
{
        if (mod->inst != NULL) {
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
                if (state->opts.print_stats) {
                        nbio_printf("instance memory consumption immediately "
                                    "before destroy: %zu\n",
                                    mod->instance_mctx->allocated);
                }
#endif
                instance_destroy(mod->inst);
                mod->inst = NULL;
        }
        if (mod->module != NULL) {
                module_destroy(mod->module_mctx, mod->module);
                mod->module = NULL;
        }
        if (mod->buf != NULL) {
                if (mod->buf_mapped) {
                        unmap_file(mod->buf, mod->bufsize);
                } else {
                        free(mod->buf);
                }
                mod->buf = NULL;
        }
        if (mod->name != NULL) {
                free(mod->name);
                mod->name = NULL;
        }
#if defined(TOYWASM_ENABLE_WASI_THREADS)
        if (mod->extra_import != NULL) {
                import_object_destroy(state->impobj_mctx, mod->extra_import);
                mod->extra_import = NULL;
        }
#endif
        if (mod->unresolved_functions_import != NULL) {
                import_object_destroy(state->impobj_mctx,
                                      mod->unresolved_functions_import);
                mod->unresolved_functions_import = NULL;
        }
        assert((mod->instance_mctx == NULL) == (mod->module_mctx == NULL));
        if (mod->module_mctx != NULL) {
                assert(mod->module_mctx + 1 == mod->instance_mctx);
                mem_context_clear(mod->instance_mctx);
                mem_context_clear(mod->module_mctx);
                mem_free(state->mctx, mod->module_mctx,
                         2 * sizeof(*mod->module_mctx));
        }
        mod->module_mctx = NULL;
        mod->instance_mctx = NULL;
}

static void
repl_unload_u(struct repl_state *state, struct repl_module_state_u *mod_u)
{
#if defined(TOYWASM_ENABLE_DYLD)
        if (state->opts.enable_dyld) {
                struct dyld *d = &mod_u->u.dyld;
#if defined(TOYWASM_ENABLE_DYLD)
                if (state->opts.print_stats) {
                        nbio_printf("=== dyld memory consumption immediately "
                                    "before dyld_clear ===\n");
                        dyld_print_stats(d);
                }
#endif
                dyld_clear(d);
                return;
        }
#endif
        struct repl_module_state *mod = &mod_u->u.repl;
        repl_unload(state, mod);
}

static void
print_memory_usage(const struct mem_context *mctx, const char *label)
{
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
#if defined(TOYWASM_ENABLE_HEAP_TRACKING_PEAK)
        nbio_printf("%23s %12zu (peak %12zu)\n", label, mctx->allocated,
                    mctx->peak);
#else
        nbio_printf("%23s %12zu\n", label, mctx->allocated);
#endif
#endif
}

void
toywasm_repl_reset(struct repl_state *state)
{
        if (state->opts.print_stats) {
                nbio_printf("=== memory consumption immediately before a repl "
                            "reset ===\n");
                print_memory_usage(state->mctx, "total");
                print_memory_usage(state->modules_mctx, "modules");
                print_memory_usage(state->instances_mctx, "instances");
                print_memory_usage(state->wasi_mctx, "wasi");
                print_memory_usage(state->dyld_mctx, "dyld");
                print_memory_usage(state->impobj_mctx, "impobj");
        }
        uint32_t n = 0;
        while (state->imports != NULL) {
                struct import_object *im = state->imports;
                state->imports = im->next;
                import_object_destroy(state->impobj_mctx, im);
                n++;
        }
        while (state->nregister > 0) {
                struct registered_name *rname = state->registered_names;
                assert(rname != NULL);
                state->registered_names = rname->next;
                state->nregister--;
                free((void *)rname->name.data);
                free(rname);
                n--;
        }
        assert(state->registered_names == NULL);
        struct repl_module_state_u *mod_u;
        VEC_FOREACH(mod_u, state->modules) {
                repl_unload_u(state, mod_u);
        }
        VEC_FREE(state->mctx, state->modules);
        VEC_FREE(state->mctx, state->param);
        VEC_FREE(state->mctx, state->result);

#if defined(TOYWASM_ENABLE_WASI_THREADS)
        if (state->wasi_threads != NULL) {
                wasi_threads_instance_destroy(state->wasi_threads);
                state->wasi_threads = NULL;
                n--;
        }
#endif
#if defined(TOYWASM_ENABLE_WASI)
        if (state->wasi != NULL) {
                wasi_instance_destroy(state->wasi);
                state->wasi = NULL;
                n--;
        }
        struct wasi_vfs **vfsp;
        VEC_FOREACH(vfsp, state->vfses) {
                int ret = wasi_vfs_fs_umount(*vfsp);
                if (ret != 0) {
                        /* log and ignore */
                        xlog_error("%s: wasi_vfs_fs_umount failed with %d",
                                   __func__, ret);
                }
        }
        VEC_FREE(state->mctx, state->vfses);
#endif
        assert(n == 0);
}

int
toywasm_repl_load_wasi(struct repl_state *state)
{
#if defined(TOYWASM_ENABLE_WASI)
        if (state->wasi != NULL) {
                xlog_error("wasi is already loaded");
                return EPROTO;
        }
        int ret;
        ret = wasi_instance_create(state->wasi_mctx, &state->wasi);
        if (ret != 0) {
                goto fail;
        }
        ret = wasi_instance_populate_stdio_with_hostfd(state->wasi);
        if (ret != 0) {
                goto undo_wasi_create;
        }
        struct import_object *im;
        ret = import_object_create_for_wasi(state->impobj_mctx, state->wasi,
                                            &im);
        if (ret != 0) {
                goto undo_wasi_create;
        }
        im->next = state->imports;
        state->imports = im;
#endif
#if defined(TOYWASM_ENABLE_WASI_THREADS)
        assert(state->wasi_threads == NULL);
        ret = wasi_threads_instance_create(state->wasi_mctx,
                                           &state->wasi_threads);
        if (ret != 0) {
                goto undo_wasi;
        }
        ret = import_object_create_for_wasi_threads(state->impobj_mctx,
                                                    state->wasi_threads, &im);
        if (ret != 0) {
                goto undo_wasi_threads_create;
        }
        im->next = state->imports;
        state->imports = im;
#endif
        return 0;

#if defined(TOYWASM_ENABLE_WASI_THREADS)
undo_wasi_threads_create:
        wasi_threads_instance_destroy(state->wasi_threads);
        state->wasi_threads = NULL;
undo_wasi:
        assert(state->wasi != NULL);
        im = state->imports;
        state->imports = im->next;
        import_object_destroy(state->impobj_mctx, im);
#endif
#if defined(TOYWASM_ENABLE_WASI)
undo_wasi_create:
        wasi_instance_destroy(state->wasi);
        state->wasi = NULL;
fail:
        xlog_error("failed to load wasi with error %u", ret);
        return ret;
#endif
}

#if defined(TOYWASM_ENABLE_WASI)
int
toywasm_repl_set_wasi_args(struct repl_state *state, int argc,
                           const char *const *argv)
{
        if (state->wasi == NULL) {
                return EPROTO;
        }
        wasi_instance_set_args(state->wasi, argc, argv);
        return 0;
}

int
toywasm_repl_set_wasi_environ(struct repl_state *state, int nenvs,
                              const char *const *envs)
{
        if (state->wasi == NULL) {
                return EPROTO;
        }
        wasi_instance_set_environ(state->wasi, nenvs, envs);
        return 0;
}

int
toywasm_repl_set_wasi_prestat(struct repl_state *state, const char *path)
{
        if (state->wasi == NULL) {
                return EPROTO;
        }
        return wasi_instance_prestat_add(state->wasi, path);
}
#endif

#if defined(TOYWASM_ENABLE_WASI_LITTLEFS)
int
toywasm_repl_set_wasi_prestat_littlefs(struct repl_state *state,
                                       const char *path)
{
        if (state->wasi == NULL) {
                return EPROTO;
        }
        int ret;
        ret = VEC_PREALLOC(state->mctx, state->vfses, 1);
        if (ret != 0) {
                return ret;
        }
        struct wasi_vfs *vfs;
        ret = wasi_instance_prestat_add_littlefs(
                state->wasi, path, &state->opts.wasi_littlefs_mount_cfg, &vfs);
        if (ret != 0) {
                return ret;
        }
        *VEC_PUSH(state->vfses) = vfs;
        return 0;
}
#endif

int
find_mod_u(struct repl_state *state, const char *modname,
           struct repl_module_state_u **modp)
{
        if (state->modules.lsize == 0) {
                xlog_printf("no module loaded\n");
                return EPROTO;
        }
        if (modname == NULL) {
                *modp = &VEC_LASTELEM(state->modules);
                return 0;
        }
#if defined(TOYWASM_ENABLE_DYLD)
        if (state->opts.enable_dyld) {
                return ENOTSUP;
        }
#endif
        struct repl_module_state_u *mod_u;
        VEC_FOREACH(mod_u, state->modules) {
                struct repl_module_state *mod = &mod_u->u.repl;
                if (mod->name != NULL && !strcmp(modname, mod->name)) {
                        *modp = mod_u;
                        return 0;
                }
        }
        return ENOENT;
}

int
find_mod(struct repl_state *state, const char *modname,
         struct repl_module_state **modp)
{
#if defined(TOYWASM_ENABLE_DYLD)
        if (state->opts.enable_dyld) {
                return ENOTSUP;
        }
#endif
        struct repl_module_state_u *mod_u;
        int ret = find_mod_u(state, modname, &mod_u);
        if (ret != 0) {
                return ret;
        }
        *modp = &mod_u->u.repl;
        return 0;
}

void
print_trap(const struct exec_context *ctx, const struct trap_info *trap)
{
        /* the messages here are aimed to match assert_trap in wast */
        enum trapid id = trap->trapid;
        const char *msg = "unknown";
        const char *trapmsg = report_getmessage(ctx->report);
        switch (id) {
        case TRAP_DIV_BY_ZERO:
                msg = "integer divide by zero";
                break;
        case TRAP_INTEGER_OVERFLOW:
                msg = "integer overflow";
                break;
        case TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS:
        case TRAP_OUT_OF_BOUNDS_DATA_ACCESS:
                msg = "out of bounds memory access";
                break;
        case TRAP_OUT_OF_BOUNDS_TABLE_ACCESS:
        case TRAP_OUT_OF_BOUNDS_ELEMENT_ACCESS:
                msg = "out of bounds table access";
                break;
        case TRAP_CALL_INDIRECT_NULL_FUNCREF:
                msg = "uninitialized element";
                break;
        case TRAP_TOO_MANY_FRAMES:
        case TRAP_TOO_MANY_STACKCELLS:
                msg = "stack overflow";
                break;
        case TRAP_CALL_INDIRECT_OUT_OF_BOUNDS_TABLE_ACCESS:
                msg = "undefined element";
                break;
        case TRAP_CALL_INDIRECT_FUNCTYPE_MISMATCH:
                msg = "indirect call type mismatch";
                break;
        case TRAP_UNREACHABLE:
                msg = "unreachable executed";
                break;
        case TRAP_INVALID_CONVERSION_TO_INTEGER:
                msg = "invalid conversion to integer";
                break;
        case TRAP_ATOMIC_WAIT_ON_NON_SHARED_MEMORY:
                msg = "expected shared memory";
                break;
        case TRAP_UNALIGNED_ATOMIC_OPERATION:
                msg = "unaligned atomic";
                break;
        case TRAP_UNCAUGHT_EXCEPTION:
                msg = "uncaught exception";
                break;
        default:
                break;
        }
        nbio_printf("Error: [trap] %s (%u): %s\n", msg, id, trapmsg);

        print_trace(ctx); /* XXX nbio_printf */
}

static void
setup_timeout(struct exec_context *ctx)
{
        /*
         * REVISIT: this timeout logic is a bit broken because it
         * assumes that, when the main thread exits, other threads
         * also exit soon.
         * right now, it happens to be true because
         * wasi_threads_complete_exec terminates other threads
         * for proc_exit anyway.
         * (see the comment in wasi_threads_instance_join.)
         *
         * possible fixes:
         * a. make wasi_threads_instance_join check timeout expiration
         * b. make the user interrutpt a cluster-wide event and handle
         *    it in non-main threads as well
         * c. give up implementing a timeout this way
         */

        const static atomic_uint one = 1;
        /*
         * keep the interrupt triggered so that we can check
         * timeout in the execution loop below.
         *
         * Note: if a host environment has a nice timer functionality
         * like alarm(3), you can make this more efficient by
         * requesting an interrupt only after timeout_ms. we don't
         * bother to make such an optimization here though because
         * we aim to be portable to wasm32-wasi, which doesn't have
         * signals.
         */
        ctx->intrp = &one;
        /*
         * a hack to avoid busy loop.
         * maybe it's cleaner for us to provide a timer functionality
         * by ourselves. but i feel it's too much for now.
         */
        ctx->user_intr_delay = 1;
}

static int
check_timeout(const struct timespec *abstimeout)
{
        struct timespec now;
        int ret = timespec_now(CLOCK_MONOTONIC, &now);
        if (ret != 0) {
                goto fail;
        }
        if (timespec_cmp(&now, abstimeout) > 0) {
                xlog_error("execution timed out");
                ret = ETIMEDOUT;
                goto fail;
        }
        ret = 0;
fail:
        return ret;
}

static int
repl_exec_init(struct repl_state *state, struct repl_module_state *mod,
               bool trap_ok)
{
        const bool has_timeout = state->has_timeout;
        struct exec_context ctx0;
        struct exec_context *ctx = &ctx0;
        int ret;
        exec_context_init(ctx, mod->inst, mod->instance_mctx);
        ctx->options = state->opts.exec_options;
        if (has_timeout) {
                setup_timeout(ctx);
        }
        ret = instance_execute_init(ctx);
        do {
                if (ret == ETOYWASMUSERINTERRUPT) {
                        assert(has_timeout);
                        int ret1 = check_timeout(&state->abstimeout);
                        if (ret1 != 0) {
                                print_trace(ctx);
                                ret = ret1;
                                goto fail;
                        }
                }
                ret = instance_execute_handle_restart_once(ctx, ret);
        } while (IS_RESTARTABLE(ret));
        if (ret == ETOYWASMTRAP) {
                assert(ctx->trapped);
                print_trap(ctx, &ctx->trap);
                if (trap_ok) {
                        ret = 0;
                }
        }
fail:
        exec_context_clear(ctx);
        return ret;
}

static void
set_memory(struct repl_state *state, struct meminst *mem)
{
#if defined(TOYWASM_ENABLE_WASI)
        if (state->wasi != NULL) {
                wasi_instance_set_memory(state->wasi, mem);
        }
#if defined(TOYWASM_ENABLE_WASI_THREADS)
        if (state->wasi_threads != NULL) {
                wasi_threads_instance_set_memory(state->wasi_threads, mem);
        }
#endif
#endif
}

static int
repl_load_from_buf(struct repl_state *state, const char *modname,
                   struct repl_module_state *mod, bool trap_ok)
{
        int ret;
#if defined(TOYWASM_ENABLE_DYLD)
        if (state->opts.enable_dyld) {
                return ENOTSUP;
        }
#endif
        struct mem_context *mctx2 =
                mem_alloc(state->mctx, 2 * sizeof(struct mem_context));
        if (mctx2 == NULL) {
                return ENOMEM;
        }
        mod->module_mctx = mctx2;
        mod->instance_mctx = mctx2 + 1;
        mem_context_init(mod->module_mctx);
        mem_context_init(mod->instance_mctx);
        mod->module_mctx->parent = state->modules_mctx;
        mod->instance_mctx->parent = state->instances_mctx;
        struct load_context ctx;
        load_context_init(&ctx, mod->module_mctx);
        ctx.options = state->opts.load_options;
        ret = module_create(&mod->module, mod->buf, mod->buf + mod->bufsize,
                            &ctx);
        if (ret != 0) {
                const char *msg = report_getmessage(&ctx.report);
                xlog_error("load/validation error: %s", msg);
                nbio_printf("load/validation error: %s\n", msg);
        }
        load_context_clear(&ctx);
        if (ret != 0) {
                xlog_printf("module_load failed\n");
                goto fail;
        }
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
        if (state->opts.print_stats) {
                nbio_printf("module memory overhead: %zu\n",
                            mod->module_mctx->allocated);
        }
#endif

        struct import_object *imports = state->imports;
#if defined(TOYWASM_ENABLE_WASI_THREADS)
        if (state->wasi_threads != NULL) {
                assert(mod->extra_import == NULL);
                /* create matching shared memory automatically */
                struct import_object *imo;
                ret = create_satisfying_shared_memories(state->impobj_mctx,
                                                        mod->instance_mctx,
                                                        mod->module, &imo);
                if (ret != 0) {
                        goto fail;
                }
                mod->extra_import = imo;
                imo->next = imports;
                imports = imo;
        }
#endif
        struct import_object **tailp = NULL;
        if (state->opts.allow_unresolved_functions) {
                struct import_object *imo;
                ret = create_satisfying_functions(state->impobj_mctx,
                                                  mod->module, &imo);
                if (ret != 0) {
                        goto fail;
                }
                mod->unresolved_functions_import = imo;
                /* put this at the end of the list */
                tailp = &imports;
                while (*tailp != NULL) {
                        tailp = &(*tailp)->next;
                }
                *tailp = imo;
        }

        struct report report;
        report_init(&report);
        ret = instance_create_no_init(mod->instance_mctx, mod->module,
                                      &mod->inst, imports, &report);
        if (tailp != NULL) {
                *tailp = NULL;
        }
        if (ret != 0) {
                const char *msg = report_getmessage(&report);
                xlog_error("instance_create_no_init failed with %d: %s", ret,
                           msg);
                nbio_printf("instantiation error: %s\n", msg);
                report_clear(&report);
                goto fail;
        }
        set_memory(state, cconv_memory(mod->inst));
        ret = repl_exec_init(state, mod, trap_ok);
        if (ret != 0) {
                xlog_printf("repl_exec_init failed\n");
                goto fail;
        }
#if defined(TOYWASM_ENABLE_HEAP_TRACKING)
        if (state->opts.print_stats) {
                nbio_printf("instance memory consumption immediately after "
                            "instantiation: %zu\n",
                            mod->instance_mctx->allocated);
        }
#endif
        if (modname != NULL) {
                mod->name = strdup(modname);
                if (mod->name == NULL) {
                        ret = ENOMEM;
                        goto fail;
                }
        }
        ret = 0;
fail:
        return ret;
}

int
toywasm_repl_load(struct repl_state *state, const char *modname,
                  const char *filename, bool trap_ok)
{
        int ret;
        ret = VEC_PREALLOC(state->mctx, state->modules, 1);
        if (ret != 0) {
                return ret;
        }
        struct repl_module_state_u *mod_u = &VEC_NEXTELEM(state->modules);
#if defined(TOYWASM_ENABLE_DYLD)
        if (state->opts.enable_dyld) {
                if (trap_ok) {
                        return ENOTSUP; /* not implemented */
                }
                struct dyld *d = &mod_u->u.dyld;
                dyld_init(d, state->dyld_mctx);
                d->opts = state->opts.dyld_options;
                d->opts.base_import_obj = state->imports;
                ret = dyld_load(d, filename);
                if (ret != 0) {
                        return ret;
                }
                set_memory(state, dyld_memory(d));
                ret = dyld_execute_init_funcs(d);
                if (ret != 0) {
                        dyld_clear(d);
                        return ret;
                }
                state->modules.lsize++;
                return 0;
        }
#endif
        struct repl_module_state *mod = &mod_u->u.repl;
        memset(mod, 0, sizeof(*mod));
        ret = map_file(filename, (void **)&mod->buf, &mod->bufsize);
        if (ret != 0) {
                xlog_error("failed to map %s (error %d)", filename, ret);
                goto fail;
        }
        mod->buf_mapped = true;
        ret = repl_load_from_buf(state, modname, mod, trap_ok);
        if (ret != 0) {
                goto fail;
        }
        state->modules.lsize++;
        return 0;
fail:
        repl_unload(state, mod);
        return ret;
}

int
toywasm_repl_load_hex(struct repl_state *state, const char *modname,
                      const char *opt)
{
#if defined(TOYWASM_ENABLE_DYLD)
        if (state->opts.enable_dyld) {
                return ENOTSUP;
        }
#endif
        int ret;
        ret = VEC_PREALLOC(state->mctx, state->modules, 1);
        if (ret != 0) {
                return ret;
        }
        struct repl_module_state_u *mod_u = &VEC_NEXTELEM(state->modules);
        struct repl_module_state *mod = &mod_u->u.repl;
        memset(mod, 0, sizeof(*mod));
        size_t sz = atoi(opt);
        mod->bufsize = sz;
        mod->buf = malloc(mod->bufsize);
        if (mod->buf == NULL) {
                ret = ENOMEM;
                goto fail;
        }
        mod->buf_mapped = false;
        xlog_printf("reading %zu bytes from stdin\n", mod->bufsize);
        ret = read_hex_from_stdin(mod->buf, mod->bufsize);
        if (ret != 0) {
                xlog_printf("failed to read module from stdin\n");
                goto fail;
        }
        ret = repl_load_from_buf(state, modname, mod, true);
        if (ret != 0) {
                goto fail;
        }
        state->modules.lsize++;
        return 0;
fail:
        repl_unload(state, mod);
        return ret;
}

static int
repl_save(struct repl_state *state, const char *modname, const char *filename)
{
#if defined(TOYWASM_ENABLE_WRITER)
        if (state->modules.lsize == 0) {
                return EPROTO;
        }
        struct repl_module_state *mod;
        int ret;
        ret = find_mod(state, modname, &mod);
        if (ret != 0) {
                goto fail;
        }
        ret = module_write(filename, mod->module);
        if (ret != 0) {
                xlog_error("failed to write module %s (error %d)", filename,
                           ret);
                goto fail;
        }
        ret = 0;
fail:
        return ret;
#else
        return ENOTSUP;
#endif
}

int
toywasm_repl_register(struct repl_state *state, const char *modname,
                      const char *register_name)
{
        int ret;
        if (state->modules.lsize == 0) {
                return EPROTO;
        }
        struct repl_module_state *mod;
        ret = find_mod(state, modname, &mod);
        if (ret != 0) {
                goto fail;
        }
        struct instance *inst = mod->inst;
        assert(inst != NULL);
        struct import_object *im;
        char *register_modname1 = strdup(register_name);
        struct registered_name *rname = malloc(sizeof(*rname));
        if (register_modname1 == NULL || rname == NULL) {
                free(rname);
                free(register_modname1);
                ret = ENOMEM;
                goto fail;
        }
        struct name *name = &rname->name;
        set_name_cstr(name, register_modname1);
        ret = import_object_create_for_exports(state->impobj_mctx, inst, name,
                                               &im);
        if (ret != 0) {
                free(rname);
                free(register_modname1);
                goto fail;
        }
        im->next = state->imports;
        state->imports = im;
        rname->next = state->registered_names;
        state->registered_names = rname;
        state->nregister++;
        ret = 0;
fail:
        return ret;
}

#if defined(TOYWASM_ENABLE_WASM_SIMD)
int
parse_v128(const char *s, struct val *result)
{
        uint64_t upper;
        uint64_t lower;
        int ret;
        if (strlen(s) != 32) {
                return EINVAL;
        }
        ret = sscanf(s, "%016" SCNx64 "%016" SCNx64, &upper, &lower);
        if (ret != 2) {
                return EINVAL;
        }
        le64_encode(&result->u.v128.i64[1], upper);
        le64_encode(&result->u.v128.i64[0], lower);
        return 0;
}
#endif

int
arg_conv(enum valtype type, const char *s, struct val *result)
{
        uintmax_t u;
        int ret;
#if defined(__GNUC__) && !defined(__clang__)
        u = 0;
#endif
        memset(result, 0, sizeof(*result));
        switch (type) {
        case TYPE_i32:
        case TYPE_f32:
                ret = str_to_uint(s, 0, &u);
                if (ret == 0) {
                        result->u.i32 = u;
                }
                break;
        case TYPE_i64:
        case TYPE_f64:
                ret = str_to_uint(s, 0, &u);
                if (ret == 0) {
                        result->u.i64 = u;
                }
                break;
#if defined(TOYWASM_ENABLE_WASM_SIMD)
        case TYPE_v128:
                ret = parse_v128(s, result);
                break;
#endif
        case TYPE_funcref:
                ret = str_to_ptr(s, 0, &u);
                if (ret != 0) {
                        break;
                }
                if (u > UINTPTR_MAX) {
                        ret = EINVAL;
                        break;
                }
                result->u.funcref.func = (void *)(uintptr_t)u;
                break;
        case TYPE_externref:
                ret = str_to_ptr(s, 0, &u);
                if (ret != 0) {
                        break;
                }
                if (u > UINTPTR_MAX) {
                        ret = EINVAL;
                        break;
                }
                result->u.externref = (void *)(uintptr_t)u;
                break;
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
                /* REVISIT: what to do for TYPE_exnref? */
#endif
        default:
                xlog_printf("arg_conv: unimplementd type %02x\n", type);
                ret = ENOTSUP;
                break;
        }
        return ret;
}

static int
repl_print_result(const struct resulttype *rt, const struct val *vals)
{
        const char *sep = "";
        uint32_t i;
        int ret = 0;
        if (rt->ntypes == 0) {
                nbio_printf("Result: <Empty Stack>\n");
                return 0;
        }
        nbio_printf("Result: ");
        for (i = 0; i < rt->ntypes; i++) {
                enum valtype type = rt->types[i];
                const struct val *val = &vals[i];
                switch (type) {
                case TYPE_i32:
                        nbio_printf("%s%" PRIu32 ":i32", sep, val->u.i32);
                        break;
                case TYPE_f32:
                        nbio_printf("%s%" PRIu32 ":f32", sep, val->u.i32);
                        break;
                case TYPE_i64:
                        nbio_printf("%s%" PRIu64 ":i64", sep, val->u.i64);
                        break;
                case TYPE_f64:
                        nbio_printf("%s%" PRIu64 ":f64", sep, val->u.i64);
                        break;
#if defined(TOYWASM_ENABLE_WASM_SIMD)
                case TYPE_v128:
                        nbio_printf("%s%016" PRIx64 "%016" PRIx64 ":v128", sep,
                                    le64_decode(&val->u.v128.i64[1]),
                                    le64_decode(&val->u.v128.i64[0]));
                        break;
#endif
                case TYPE_funcref:
                        if (val->u.funcref.func == NULL) {
                                nbio_printf("%snull:funcref", sep);
                        } else {
                                nbio_printf("%s%" PRIuPTR ":funcref", sep,
                                            (uintptr_t)val->u.funcref.func);
                        }
                        break;
                case TYPE_externref:
                        if ((uintptr_t)val->u.externref == EXTERNREF_0) {
                                nbio_printf("%s0:externref", sep);
                        } else if (val->u.externref == NULL) {
                                nbio_printf("%snull:externref", sep);
                        } else {
                                nbio_printf("%s%" PRIuPTR ":externref", sep,
                                            (uintptr_t)val->u.externref);
                        }
                        break;
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
                case TYPE_exnref:
                        if (val->u.exnref.tag == NULL) {
                                nbio_printf("%snull:exnref", sep);
                        } else {
                                nbio_printf("%s%" PRIuPTR ":exnref", sep,
                                            (uintptr_t)val->u.exnref.tag);
                        }
                        break;
#endif
                default:
                        xlog_printf("print_result: unimplementd type %02x\n",
                                    type);
                        ret = ENOTSUP;
                        break;
                }
                sep = ", ";
        }
        nbio_printf("\n");
        return ret;
}

int
unescape(char *p0, size_t *lenp)
{
        /*
         * unescape string like "\xe1\xba\x9b" in-place.
         *
         * Note: quote support here is an incomplete hack to allow
         * passing an empty name ("") on the repl prompt. (the spec
         * test has a case to examine zero-length name, which is
         * spec-wise valid.)
         * Because repl itself uses simple strtok to parse the input,
         * things like "a b" don't work on the prompt as you might
         * expect. "a\x20b" can work better.
         */
        bool in_quote = false;
        char *p = p0;
        char *wp = p;
        while (*p != 0) {
                if (in_quote) {
                        if (p[0] == '"') {
                                in_quote = false;
                                p++;
                                continue;
                        }
                } else {
                        if (p[0] == '"') {
                                in_quote = true;
                                p++;
                                continue;
                        }
                }
                if (p[0] == '\\') {
                        if (p[1] == 'x') {
                                p += 2;
                                char buf[3];
                                if ((buf[0] = *p++) == 0) {
                                        return EINVAL;
                                }
                                if ((buf[1] = *p++) == 0) {
                                        return EINVAL;
                                }
                                buf[2] = 0;
                                uintmax_t v;
#if defined(__GNUC__) && !defined(__clang__)
                                v = 0;
#endif
                                int ret = str_to_uint(buf, 16, &v);
                                if (ret != 0) {
                                        return ret;
                                }
                                *wp++ = (char)v;
                        } else {
                                return EINVAL;
                        }
                } else {
                        *wp++ = *p++;
                }
        }
        if (in_quote) {
                return EINVAL;
        }
        *lenp = wp - p0;
        *wp++ = 0;
        return 0;
}

int
toywasm_repl_set_timeout(struct repl_state *state, int timeout_ms)
{
        int ret = abstime_from_reltime_ms(CLOCK_MONOTONIC, &state->abstimeout,
                                          timeout_ms);
        if (ret != 0) {
                return ret;
        }
        state->has_timeout = true;
        return 0;
}

static int
exec_func(struct exec_context *ctx, uint32_t funcidx,
          const struct resulttype *ptype, const struct resulttype *rtype,
          const struct val *param, struct val *result,
          const struct timespec *abstimeout, const struct trap_info **trapp)
{
        int ret;
        *trapp = NULL;
        if (abstimeout != NULL) {
                setup_timeout(ctx);
        }
        assert(ctx->stack.lsize == 0);
        ret = exec_push_vals(ctx, ptype, param);
        if (ret != 0) {
                goto fail;
        }
        ret = instance_execute_func(ctx, funcidx, ptype, rtype);
        do {
                if (ret == ETOYWASMUSERINTERRUPT) {
                        assert(abstimeout != NULL);
                        int ret1 = check_timeout(abstimeout);
                        if (ret1 != 0) {
                                print_trace(ctx);
                                ret = ret1;
                                goto fail;
                        }
                }
                ret = instance_execute_handle_restart_once(ctx, ret);
        } while (IS_RESTARTABLE(ret));
        if (ret == ETOYWASMTRAP) {
                assert(ctx->trapped);
                const struct trap_info *trap = &ctx->trap;
                *trapp = trap;
        } else if (ret == 0) {
                exec_pop_vals(ctx, rtype, result);
                assert(ctx->stack.lsize == 0);
        }
fail:
        return ret;
}

/*
 * "cmd" is like "add 1 2"
 */
int
toywasm_repl_invoke(struct repl_state *state, const char *modname,
                    const char *cmd, uint32_t *exitcodep, bool print_result)
{
        char *cmd1 = strdup(cmd);
        if (cmd1 == NULL) {
                return ENOMEM;
        }
        int ret;
        /* TODO handle quote */
        char *funcname = strtok(cmd1, " ");
        if (funcname == NULL) {
                xlog_printf("no func name\n");
                ret = EPROTO;
                goto fail;
        }
        xlog_trace("repl: invoke func %s", funcname);
        size_t len;
        ret = unescape(funcname, &len);
        if (ret != 0) {
                xlog_error("failed to unescape funcname");
                goto fail;
        }
        struct name funcname_name;
        funcname_name.data = funcname;
        funcname_name.nbytes = len;
        struct repl_module_state_u *mod_u;
        ret = find_mod_u(state, modname, &mod_u);
        if (ret != 0) {
                goto fail;
        }
        struct instance *inst;
        struct mem_context *mctx;
#if defined(TOYWASM_ENABLE_DYLD)
        if (state->opts.enable_dyld) {
                struct dyld *d = &mod_u->u.dyld;
                inst = dyld_main_object_instance(d);
                mctx = state->mctx;
        } else
#endif
        {
                struct repl_module_state *mod = &mod_u->u.repl;
                inst = mod->inst;
                mctx = mod->instance_mctx;
        }
        const struct module *module = inst->module;
        assert(inst != NULL);
        assert(module != NULL);
        uint32_t funcidx;
        ret = module_find_export(module, &funcname_name, EXTERNTYPE_FUNC,
                                 &funcidx);
        if (ret != 0) {
                /* TODO should print the name w/o unescape */
                xlog_error("module_find_export failed for %s", funcname);
                goto fail;
        }
        const struct functype *ft = module_functype(module, funcidx);
        const struct resulttype *ptype = &ft->parameter;
        const struct resulttype *rtype = &ft->result;
        ret = VEC_RESIZE(state->mctx, state->param, ptype->ntypes);
        if (ret != 0) {
                goto fail;
        }
        ret = VEC_RESIZE(state->mctx, state->result, rtype->ntypes);
        if (ret != 0) {
                goto fail;
        }
        struct val *param = &VEC_ELEM(state->param, 0);
        struct val *result = &VEC_ELEM(state->result, 0);
        uint32_t i;
        for (i = 0; i < ptype->ntypes; i++) {
                char *arg = strtok(NULL, " ");
                if (arg == NULL) {
                        xlog_printf("missing arg\n");
                        ret = EPROTO;
                        goto fail;
                }
                ret = arg_conv(ptype->types[i], arg, &param[i]);
                if (ret != 0) {
                        xlog_printf("arg_conv failed\n");
                        goto fail;
                }
        }
        if (strtok(NULL, " ") != NULL) {
                xlog_printf("extra arg\n");
                ret = EPROTO;
                goto fail;
        }
        struct exec_context ctx0;
        struct exec_context *ctx = &ctx0;
        exec_context_init(ctx, inst, mctx);
        ctx->options = state->opts.exec_options;
        const struct trap_info *trap;
#if defined(TOYWASM_ENABLE_WASI_THREADS)
        struct wasi_threads_instance *wasi_threads = state->wasi_threads;
        wasi_threads_setup_exec_context(wasi_threads, ctx);
#endif
        const struct timespec *abstimeout =
                state->has_timeout ? &state->abstimeout : NULL;
        ret = exec_func(ctx, funcidx, ptype, rtype, param, result, abstimeout,
                        &trap);
#if defined(TOYWASM_ENABLE_WASI_THREADS)
        wasi_threads_complete_exec(wasi_threads, &trap);
#endif
        if (state->opts.print_stats) {
                exec_context_print_stats(ctx);
                instance_print_stats(inst);
        }

        if (ret == ETOYWASMTRAP) {
                assert(trap != NULL);
#if defined(TOYWASM_ENABLE_WASI)
                if (trap->trapid == TRAP_VOLUNTARY_EXIT) {
                        struct wasi_instance *wasi = state->wasi;
                        /* Note: TRAP_VOLUNTARY_EXIT is only used by wasi */
                        assert(wasi != NULL);
                        uint32_t exit_code = wasi_instance_exit_code(wasi);
                        xlog_trace("voluntary exit (%" PRIu32 ")", exit_code);
                        if (exitcodep != NULL) {
                                *exitcodep = exit_code;
                                ret = 0;
                        } else {
                                ret = exit_code;
                        }
                        exec_context_clear(ctx);
                        goto fail;
                }
#endif
                print_trap(ctx, trap);
        }
        exec_context_clear(ctx);
        if (ret != 0) {
                xlog_printf("instance_execute_func failed with %d\n", ret);
                goto fail;
        }
        if (print_result) {
                ret = repl_print_result(rtype, result);
                if (ret != 0) {
                        xlog_printf("print_result failed\n");
                        goto fail;
                }
        }
        if (exitcodep != NULL) {
                *exitcodep = 0;
        }
        ret = 0;
fail:
        free(cmd1);
        return ret;
}

static int
repl_global_get(struct repl_state *state, const char *modname,
                const char *name_cstr)
{
        char *name1 = strdup(name_cstr);
        if (name1 == NULL) {
                return ENOMEM;
        }
        size_t len;
        int ret;
        ret = unescape(name1, &len);
        if (ret != 0) {
                xlog_error("failed to unescape name");
                goto fail;
        }
        struct name name;
        name.data = name1;
        name.nbytes = len;
        struct repl_module_state *mod;
        ret = find_mod(state, modname, &mod);
        if (ret != 0) {
                goto fail;
        }
        struct instance *inst = mod->inst;
        struct module *module = mod->module;
        assert(inst != NULL);
        assert(module != NULL);
        uint32_t idx;
        ret = module_find_export(module, &name, EXTERNTYPE_GLOBAL, &idx);
        if (ret != 0) {
                xlog_error("module_find_export failed for %s", name_cstr);
                goto fail;
        }
        const struct globaltype *gt = module_globaltype(module, idx);
        enum valtype type = gt->t;
        const struct resulttype rtype = {
                .types = &type,
                .ntypes = 1,
        };
        struct val val = VEC_ELEM(inst->globals, idx)->val;
        ret = repl_print_result(&rtype, &val);
        if (ret != 0) {
                xlog_printf("print_result failed\n");
                goto fail;
        }
        ret = 0;
fail:
        free(name1);
        return ret;
}

void
toywasm_repl_print_version(void)
{
        nbio_printf("toywasm %s\n", TOYWASM_VERSION);
#if defined(__clang_version__)
        nbio_printf("__clang_version__ = %s\n", __clang_version__);
#endif
#if !defined(__clang__)
#if defined(__GNUC__)
        nbio_printf("__GNUC__ = %u\n", __GNUC__);
#endif
#if defined(__GNUC_MINOR__)
        nbio_printf("__GNUC_MINOR__ = %u\n", __GNUC_MINOR__);
#endif
#if defined(__GNUC_PATCHLEVEL__)
        nbio_printf("__GNUC_PATCHLEVEL__ = %u\n", __GNUC_PATCHLEVEL__);
#endif
#endif /* !defined(__clang__) */
#if defined(__BYTE_ORDER__)
        nbio_printf("__BYTE_ORDER__ is %u (__ORDER_LITTLE_ENDIAN__ is %u)\n",
                    __BYTE_ORDER__, __ORDER_LITTLE_ENDIAN__);
#endif
        nbio_printf("sizeof(void *) = %zu\n", sizeof(void *));
#if defined(__wasi__)
        nbio_printf("__wasi__ defined\n");
#endif
#if defined(__x86_64__)
        nbio_printf("__x86_64__ defined\n");
#endif
#if defined(__aarch64__)
        nbio_printf("__aarch64__ defined\n");
#endif
#if defined(__arm__)
        nbio_printf("__arm__ defined\n");
#endif
#if defined(__ppc__)
        nbio_printf("__ppc__ defined\n");
#endif
#if defined(__riscv)
        nbio_printf("__riscv defined\n");
#endif
#if defined(__s390x__)
        nbio_printf("__s390x__ defined\n");
#endif
#if defined(__s390__)
        nbio_printf("__s390__ defined\n");
#endif
#if defined(__wasm__)
        nbio_printf("__wasm__ defined\n");
#endif
#if defined(__wasm32__)
        nbio_printf("__wasm32__ defined\n");
#endif
#if defined(__wasm64__)
        nbio_printf("__wasm64__ defined\n");
#endif
#if defined(__APPLE__)
        nbio_printf("__APPLE__ defined\n");
#endif
#if defined(__NuttX__)
        nbio_printf("__NuttX__ defined\n");
#endif
#if defined(__linux__)
        nbio_printf("__linux__ defined\n");
#endif
}

void
toywasm_repl_print_build_options(void)
{
        extern const char *toywasm_config_string;
        nbio_printf("%s", toywasm_config_string);
}

static int
repl_module_subcmd(struct repl_state *state, const char *cmd,
                   const char *modname, const char *opt)
{
        int ret;

        if (!strcmp(cmd, "load") && opt != NULL) {
                ret = toywasm_repl_load(state, modname, opt, true);
                if (ret != 0) {
                        goto fail;
                }
        } else if (!strcmp(cmd, "load-hex") && opt != NULL) {
                ret = toywasm_repl_load_hex(state, modname, opt);
                if (ret != 0) {
                        goto fail;
                }
        } else if (!strcmp(cmd, "invoke") && opt != NULL) {
                ret = toywasm_repl_invoke(state, modname, opt, NULL, true);
                if (ret != 0) {
                        goto fail;
                }
        } else if (!strcmp(cmd, "register") && opt != NULL) {
                ret = toywasm_repl_register(state, modname, opt);
                if (ret != 0) {
                        goto fail;
                }
        } else if (!strcmp(cmd, "save") && opt != NULL) {
                ret = repl_save(state, modname, opt);
                if (ret != 0) {
                        goto fail;
                }
        } else if (!strcmp(cmd, "global-get") && opt != NULL) {
                ret = repl_global_get(state, modname, opt);
                if (ret != 0) {
                        goto fail;
                }
        } else {
                xlog_printf("Error: unknown command %s\n", cmd);
                ret = 0;
        }
fail:
        return ret;
}

static void
repl_options_init(struct repl_options *opts)
{
        opts->prompt = "toywasm";
        opts->print_stats = false;
        load_options_set_defaults(&opts->load_options);
        exec_options_set_defaults(&opts->exec_options);
#if defined(TOYWASM_ENABLE_DYLD)
        dyld_options_set_defaults(&opts->dyld_options);
#endif
#if defined(TOYWASM_ENABLE_WASI_LITTLEFS)
        memset(&opts->wasi_littlefs_mount_cfg, 0,
               sizeof(opts->wasi_littlefs_mount_cfg));
#endif
}

void
toywasm_repl_state_init(struct repl_state *state)
{
        memset(state, 0, sizeof(*state));
        VEC_INIT(state->param);
        VEC_INIT(state->result);
        VEC_INIT(state->modules);
        VEC_INIT(state->vfses);
        repl_options_init(&state->opts);
}

int
toywasm_repl(struct repl_state *state)
{
        char *line = NULL;
        size_t linecap = 0;
        int ret;
        while (true) {
                nbio_printf("%s> ", state->opts.prompt);
                fflush(stdout);
                ret = nbio_getline(&line, &linecap, stdin);
                if (ret == -1) {
                        break;
                }
                xlog_printf("repl cmd '%s'\n", line);
                char *cmd = strtok(line, " \n");
                if (cmd == NULL) {
                        continue;
                }
                char *opt = strtok(NULL, "\n");
                if (!strcmp(cmd, ":version")) {
                        toywasm_repl_print_version();
                } else if (!strcmp(cmd, ":init")) {
                        toywasm_repl_reset(state);
                } else if (!strcmp(cmd, ":exit")) {
                        /*
                         * Note: an explicit exit command is useful
                         * where VEOF is not available. eg. nuttx
                         */
                        break;
                } else if (!strcmp(cmd, ":module") && opt != NULL) {
                        char *modname = strtok(opt, " ");
                        if (modname == NULL) {
                                ret = EPROTO;
                                goto fail;
                        }
                        char *subcmd = strtok(NULL, " ");
                        if (subcmd == NULL) {
                                ret = EPROTO;
                                goto fail;
                        }
                        opt = strtok(NULL, "");
                        ret = repl_module_subcmd(state, subcmd, modname, opt);
                        if (ret != 0) {
                                goto fail;
                        }
                } else if (*cmd == ':') {
                        ret = repl_module_subcmd(state, cmd + 1, NULL, opt);
                        if (ret != 0) {
                                goto fail;
                        }
                }
                continue;
fail:
                xlog_printf("repl fail with %d\n", ret);
                nbio_printf("Error: command '%s' failed with %d\n", cmd, ret);
        }
        free(line);
        toywasm_repl_reset(state);
        return 0;
}
