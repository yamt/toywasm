/*
 * Note: the main purpose of this repl implementation is to run
 * the wasm3 testsuite:
 * https://github.com/wasm3/wasm3/blob/main/test/run-spec-test.py
 *
 * eg.
 * ./run-spec-test.py --exec ".../main_bin --repl --repl-prompt wasm3"
 */

#define _GNU_SOURCE /* strdup */

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "fileio.h"
#include "instance.h"
#include "load_context.h"
#include "module.h"
#include "repl.h"
#include "report.h"
#include "type.h"
#include "wasi.h"
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
#define EXTERNREF_0 ((uintptr_t)-1)

const char *g_repl_prompt = "toywasm";
bool g_repl_use_jump_table = true;

struct repl_module_state {
        uint8_t *buf;
        size_t bufsize;
        bool buf_mapped;
        struct module *module;
        struct instance *inst;
};

/* eg. const.wast has 366 modules */
#define MAX_MODULES 500

struct repl_state {
        struct repl_module_state modules[MAX_MODULES];
        unsigned int nmodules;
        struct import_object *imports;
        unsigned int nregister;
        struct name registered_names[MAX_MODULES];
        struct val *param;
        struct val *result;
        struct wasi_instance *wasi;
};

struct repl_state g_repl_state0;
struct repl_state *g_repl_state = &g_repl_state0;

int
str_to_uint(const char *s, int base, uintmax_t *resultp)
{
        uintmax_t v;
        char *ep;
        errno = 0;
        v = strtoumax(s, &ep, base);
        if (s == ep) {
                return EINVAL;
        }
        if (*ep != 0) {
                return EINVAL;
        }
        if (errno != 0) {
                return errno;
        }
        *resultp = v;
        return 0;
}

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

void
repl_unload(struct repl_module_state *mod)
{
        if (mod->inst != NULL) {
                instance_destroy(mod->inst);
                mod->inst = NULL;
        }
        if (mod->module != NULL) {
                module_destroy(mod->module);
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
}

void
repl_reset(struct repl_state *state)
{
        uint32_t n = 0;
        while (state->imports != NULL) {
                struct import_object *im = state->imports;
                state->imports = im->next;
                import_object_destroy(im);
                n++;
        }
        while (state->nregister > 0) {
                clear_name(&state->registered_names[--state->nregister]);
                n--;
        }
        while (state->nmodules > 0) {
                repl_unload(&state->modules[--state->nmodules]);
        }
        free(state->param);
        state->param = NULL;
        free(state->result);
        state->result = NULL;

        if (state->wasi != NULL) {
                wasi_instance_destroy(state->wasi);
                state->wasi = NULL;
                n--;
        }
        assert(n == 0);
}

int
repl_load_wasi(struct repl_state *state)
{
        if (state->wasi != NULL) {
                xlog_error("wasi is already loaded");
                return EPROTO;
        }
        int ret;
        ret = wasi_instance_create(&state->wasi);
        if (ret != 0) {
                goto fail;
        }
        struct import_object *im;
        ret = import_object_create_for_wasi(state->wasi, &im);
        if (ret != 0) {
                goto fail;
        }
        im->next = state->imports;
        state->imports = im;
        return 0;
fail:
        xlog_error("failed to load wasi");
        return ret;
}

int
repl_set_wasi_args(struct repl_state *state, int argc, char *const *argv)
{
        if (state->wasi == NULL) {
                return EPROTO;
        }
        wasi_instance_set_args(state->wasi, argc, argv);
        return 0;
}

int
repl_set_wasi_prestat(struct repl_state *state, const char *path)
{
        if (state->wasi == NULL) {
                return EPROTO;
        }
        return wasi_instance_prestat_add(state->wasi, path);
}

int
repl_load_from_buf(struct repl_state *state, struct repl_module_state *mod)
{
        int ret;
        ret = module_create(&mod->module);
        if (ret != 0) {
                xlog_printf("module_create failed\n");
                goto fail;
        }
        struct load_context ctx;
        load_context_init(&ctx);
        ctx.generate_jump_table = g_repl_use_jump_table;
        ret = module_load(mod->module, mod->buf, mod->buf + mod->bufsize,
                          &ctx);
        if (ctx.report.msg != NULL) {
                xlog_error("load/validation error: %s", ctx.report.msg);
                printf("load/validation error: %s\n", ctx.report.msg);
        } else if (ret != 0) {
                printf("load/validation error: unknown\n");
        }
        load_context_clear(&ctx);
        if (ret != 0) {
                xlog_printf("module_load failed\n");
                goto fail;
        }
        struct report report;
        report_init(&report);
        ret = instance_create(mod->module, &mod->inst, state->imports,
                              &report);
        if (report.msg != NULL) {
                xlog_error("instance_create: %s", report.msg);
                printf("instantiation error: %s\n", report.msg);
        } else if (ret != 0) {
                printf("instantiation error: unknown\n");
        }
        report_clear(&report);
        if (ret != 0) {
                xlog_printf("instance_create failed\n");
                goto fail;
        }
fail:
        return ret;
}

int
repl_load(struct repl_state *state, const char *filename)
{
        if (state->nmodules == MAX_MODULES) {
                return EOVERFLOW;
        }
        struct repl_module_state *mod = &state->modules[state->nmodules];
        int ret;
        ret = map_file(filename, (void **)&mod->buf, &mod->bufsize);
        if (ret != 0) {
                xlog_error("failed to map %s (error %d)", filename, ret);
                goto fail;
        }
        mod->buf_mapped = true;
        ret = repl_load_from_buf(state, mod);
        if (ret != 0) {
                goto fail;
        }
        state->nmodules++;
        return 0;
fail:
        repl_unload(mod);
        return ret;
}

int
repl_load_hex(struct repl_state *state, size_t sz)
{
        if (state->nmodules == MAX_MODULES) {
                return EOVERFLOW;
        }
        struct repl_module_state *mod = &state->modules[state->nmodules];
        int ret;
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
        ret = repl_load_from_buf(state, mod);
        if (ret != 0) {
                goto fail;
        }
        state->nmodules++;
        return 0;
fail:
        repl_unload(mod);
        return ret;
}

int
repl_register(struct repl_state *state, const char *module_name)
{
        if (state->nmodules == 0) {
                return EPROTO;
        }
        if (state->nregister == MAX_MODULES) {
                return EOVERFLOW;
        }
        char *module_name1 = strdup(module_name);
        if (module_name1 == NULL) {
                return ENOMEM;
        }
        struct repl_module_state *mod = &state->modules[state->nmodules - 1];
        struct instance *inst = mod->inst;
        assert(inst != NULL);
        struct import_object *im;
        int ret;

        struct name *name = &state->registered_names[state->nregister];
        set_name_cstr(name, module_name1);
        ret = import_object_create_for_exports(inst, name, &im);
        if (ret != 0) {
                free(module_name1);
                return ret;
        }
        im->next = state->imports;
        state->imports = im;
        state->nregister++;
        return 0;
}

int
arg_conv(enum valtype type, const char *s, struct val *result)
{
        uintmax_t u;
        int ret;
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
        case TYPE_FUNCREF:
                ret = str_to_ptr(s, 0, &u);
                if (ret == 0) {
                        result->u.funcref.func = (void *)u;
                }
                break;
        case TYPE_EXTERNREF:
                ret = str_to_ptr(s, 0, &u);
                if (ret == 0) {
                        result->u.externref = (void *)u;
                }
                break;
        default:
                xlog_printf("arg_conv: unimplementd type %02x\n", type);
                ret = ENOTSUP;
                break;
        }
        return ret;
}

int
repl_print_result(const struct resulttype *rt, const struct val *vals)
{
        const char *sep = "";
        uint32_t i;
        int ret = 0;
        if (rt->ntypes == 0) {
                printf("Result: <Empty Stack>\n");
                return 0;
        }
        printf("Result: ");
        for (i = 0; i < rt->ntypes; i++) {
                enum valtype type = rt->types[i];
                const struct val *val = &vals[i];
                switch (type) {
                case TYPE_i32:
                        printf("%s%" PRIu32 ":i32", sep, val->u.i32);
                        break;
                case TYPE_f32:
                        printf("%s%" PRIu32 ":f32", sep, val->u.i32);
                        break;
                case TYPE_i64:
                        printf("%s%" PRIu64 ":i64", sep, val->u.i64);
                        break;
                case TYPE_f64:
                        printf("%s%" PRIu64 ":f64", sep, val->u.i64);
                        break;
                case TYPE_FUNCREF:
                        if (val->u.funcref.func == NULL) {
                                printf("%snull:funcref", sep);
                        } else {
                                printf("%s%" PRIuPTR ":funcref", sep,
                                       (uintptr_t)val->u.funcref.func);
                        }
                        break;
                case TYPE_EXTERNREF:
                        if ((uintptr_t)val->u.externref == EXTERNREF_0) {
                                printf("%s0:externref", sep);
                        } else if (val->u.externref == NULL) {
                                printf("%snull:externref", sep);
                        } else {
                                printf("%s%" PRIuPTR ":externref", sep,
                                       (uintptr_t)val->u.externref);
                        }
                        break;
                default:
                        xlog_printf("print_result: unimplementd type %02x\n",
                                    type);
                        ret = ENOTSUP;
                        break;
                }
                sep = ", ";
        }
        printf("\n");
        return ret;
}

void
print_trap(const struct exec_context *ctx)
{
        /* the messages here are aimed to match assert_trap in wast */
        enum trapid id = ctx->trapid;
        const char *msg = "unknown";
        const char *trapmsg = ctx->report->msg;
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
        case TRAP_TOO_MANY_STACKVALS:
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
        default:
                break;
        }
        if (trapmsg == NULL) {
                trapmsg = "no message";
        }
        printf("Error: [trap] %s (%u): %s\n", msg, id, trapmsg);
}

int
unescape(char *p0, size_t *lenp)
{
        /* unescape string like "\xe1\xba\x9b" in-place */

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

/*
 * "cmd" is like "add 1 2"
 */
int
repl_invoke(struct repl_state *state, const char *cmd, bool print_result)
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
        if (state->nmodules == 0) {
                xlog_printf("no module loaded\n");
                ret = EPROTO;
                goto fail;
        }
        struct repl_module_state *mod = &state->modules[state->nmodules - 1];
        struct instance *inst = mod->inst;
        struct module *module = mod->module;
        assert(inst != NULL);
        assert(module != NULL);
        uint32_t funcidx;
        ret = module_find_export_func(module, &funcname_name, &funcidx);
        if (ret != 0) {
                /* TODO should print the name w/o unescape */
                xlog_error("module_find_export_func failed for %s", funcname);
                goto fail;
        }
        const struct functype *ft = module_functype(module, funcidx);
        const struct resulttype *ptype = &ft->parameter;
        const struct resulttype *rtype = &ft->result;
        ret = ARRAY_RESIZE(state->param, ptype->ntypes);
        if (ret != 0) {
                goto fail;
        }
        ret = ARRAY_RESIZE(state->result, rtype->ntypes);
        if (ret != 0) {
                goto fail;
        }
        struct val *param = state->param;
        struct val *result = state->result;
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
        exec_context_init(ctx, inst);
        ret = instance_execute_func(ctx, &funcname_name, ptype, rtype, param,
                                    result);
        if (ret == EFAULT && ctx->trapped) {
                if (ctx->trapid == TRAP_VOLUNTARY_EXIT) {
                        xlog_trace("voluntary exit (%" PRIu32 ")",
                                   ctx->exit_code);
                        ret = ctx->exit_code;
                        goto fail;
                }
                print_trap(ctx);
        }
        exec_context_clear(ctx);
        if (ret != 0) {
                xlog_printf("instance_execute_func failed\n");
                goto fail;
        }
        if (print_result) {
                ret = repl_print_result(rtype, result);
                if (ret != 0) {
                        xlog_printf("print_result failed\n");
                        goto fail;
                }
        }
        ret = 0;
fail:
        free(cmd1);
        return ret;
}

void
repl_print_version(void)
{
        printf("toywasm wasm interpreter\n");
#if defined(__clang_version__)
        printf("__clang_version__ = %s\n", __clang_version__);
#endif
#if defined(__wasi__)
        printf("__wasi__ defined\n");
#endif
#if defined(__x86_64__)
        printf("__x86_64__ defined\n");
#endif
#if defined(__APPLE__)
        printf("__APPLE__ defined\n");
#endif
#if defined(__linux__)
        printf("__linux__ defined\n");
#endif
#if defined(USE_SEPARATE_EXECUTE)
        printf("USE_SEPARATE_EXECUTE defined\n");
#endif
#if defined(USE_TAILCALL)
        printf("USE_TAILCALL defined\n");
#endif
#if defined(ENABLE_TRACING)
        printf("ENABLE_TRACING defined\n");
#endif
}

int
repl(void)
{
        struct repl_state *state = g_repl_state;
        char *line = NULL;
        size_t linecap = 0;
        int ret;
        while (true) {
                printf("%s> ", g_repl_prompt);
                fflush(stdout);
                ret = getline(&line, &linecap, stdin);
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
                        repl_print_version();
                } else if (!strcmp(cmd, ":init")) {
                        repl_reset(state);
                } else if (!strcmp(cmd, ":load") && opt != NULL) {
                        ret = repl_load(state, opt);
                        if (ret != 0) {
                                goto fail;
                        }
                } else if (!strcmp(cmd, ":load-hex") && opt != NULL) {
                        ret = repl_load_hex(state, atoi(opt));
                        if (ret != 0) {
                                goto fail;
                        }
                } else if (!strcmp(cmd, ":invoke") && opt != NULL) {
                        ret = repl_invoke(state, opt, true);
                        if (ret != 0) {
                                goto fail;
                        }
                } else if (!strcmp(cmd, ":register") && opt != NULL) {
                        ret = repl_register(state, opt);
                        if (ret != 0) {
                                goto fail;
                        }
                } else {
                        xlog_printf("Error: unknown command %s\n", cmd);
                }
                continue;
fail:
                xlog_printf("repl fail with %d\n", ret);
                printf("Error: command '%s' failed with %d\n", cmd, ret);
        }
        free(line);
        repl_reset(state);
        return 0;
}
