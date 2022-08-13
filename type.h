#if !defined(_TYPE_H)
#define _TYPE_H

#include <stdbool.h>
#include <stdint.h>

#include "bitmap.h"
#include "vec.h"

#define WASM_PAGE_SIZE 65536
#define WASM_MAX_PAGES 65536

/*
 * REVISIT: arbitrary limits for stack overflow tests in call.wast.
 * should be configurable at least.
 */
#define MAX_FRAMES 2000
#define MAX_STACKVALS 10000

struct jump {
        uint32_t pc;
        uint32_t targetpc;
};

/* hints for execution */
struct expr_exec_info {
        uint32_t njumps;
        struct jump *jumps;

        uint32_t maxlabels; /* max labels (including the implicit one) */
        uint32_t maxvals;   /* max vals on stack */
};

struct expr {
        const uint8_t *start;
        const uint8_t *end;

        struct expr_exec_info ei;
};

enum valtype {
        /* numtype */
        TYPE_i32 = 0x7f,
        TYPE_i64 = 0x7e,
        TYPE_f32 = 0x7d,
        TYPE_f64 = 0x7c,

        /* vectype */
        TYPE_v128 = 0x7b,

        /* reftype */
        TYPE_FUNCREF = 0x70,
        TYPE_EXTERNREF = 0x6f,

        /* a pseudo type for validation logic */
        TYPE_ANYREF = 0xfe, /* any reftype */
        TYPE_UNKNOWN = 0xff,
};

struct resulttype {
        uint32_t ntypes;
        enum valtype *types;
        bool is_static;
};

struct functype {
        struct resulttype parameter;
        struct resulttype result;
};

struct funcref {
        struct funcinst *func;
};

/*
 * a value on operand stack, locals, etc.
 *
 * This fixed-sized representation allows simpler code
 * while it isn't space-efficient.
 */
struct val {
        union {
                uint32_t i32;
                uint64_t i64;
                float f32;
                double f64;
                struct funcref funcref;
                void *externref;
        } u;
};

struct localchunk {
        enum valtype type;
        uint32_t n;
};

struct func {
        uint32_t nlocals;
        uint32_t nlocalchunks;
        struct localchunk *localchunks;
        struct expr e;
};

enum element_mode {
        ELEM_MODE_ACTIVE,
        ELEM_MODE_PASSIVE,
        ELEM_MODE_DECLARATIVE,
};

struct element {
        enum valtype type;
        uint32_t init_size; /* entries in init_exprs or funcs */
        struct expr *init_exprs;
        uint32_t *funcs;
        enum element_mode mode;

        /* only for active */
        uint32_t table;
        struct expr offset;
};

enum globalmut {
        GLOBAL_CONST = 0x00,
        GLOBAL_VAR = 0x01,
};

struct globaltype {
        enum valtype t;
        enum globalmut mut;
};

struct global {
        struct globaltype type;
        struct expr init;
};

struct limits {
        uint32_t min;
        /* Note: this implementation uses max=UINT32_MAX to mean "no max" */
        uint32_t max;
};

struct tabletype {
        enum valtype et;
        struct limits lim;
};

enum importtype {
        IMPORT_FUNC = 0x00,
        IMPORT_TABLE = 0x01,
        IMPORT_MEMORY = 0x02,
        IMPORT_GLOBAL = 0x03,
};

struct importdesc {
        enum importtype type;
        union {
                uint32_t typeidx;
                struct {
                        struct limits lim;
                } memtype;
                struct tabletype tabletype;
                struct globaltype globaltype;
        } u;
};

enum exporttype {
        EXPORT_FUNC = 0x00,
        EXPORT_TABLE = 0x01,
        EXPORT_MEMORY = 0x02,
        EXPORT_GLOBAL = 0x03,
};

struct exportdesc {
        enum exporttype type;
        uint32_t idx;
};

/* Note: wasm name strings are not 0-terminated */
struct name {
        uint32_t nbytes;
        const char *data; /* utf-8 */
};

/* usage: printf("%.*s", CSTR(name)); */
/* TODO escape unprintable names */
#define CSTR(n) (int)(n)->nbytes, (n)->data

#define NAME_FROM_CSTR_LITERAL(C)                                             \
        {                                                                     \
                .nbytes = sizeof(C) - 1, .data = C,                           \
        }

struct import {
        struct name module_name;
        struct name name;
        struct importdesc desc;
};

struct export
{
        struct name name;
        struct exportdesc desc;
};

enum data_mode {
        DATA_MODE_ACTIVE,
        DATA_MODE_PASSIVE,
};

struct data {
        enum data_mode mode;
        uint32_t init_size;
        const uint8_t *init;

        /* only for active */
        uint32_t memory;
        struct expr offset;
};

/*
 * https://webassembly.github.io/spec/core/syntax/modules.html#indices
 * > The index space for functions, tables, memories and globals
 * > includes respective imports declared in the same module.
 * > The indices of these imports precede the indices of other
 * > definitions in the same index space.
 */

struct module {
        uint32_t ntypes;
        struct functype *types;

        uint32_t nimportedfuncs;
        uint32_t nfuncs;
        /* Note: functypeidxes and funcs shares indexes */
        uint32_t *functypeidxes;
        struct func *funcs;

        uint32_t nimportedtables;
        uint32_t ntables;
        struct tabletype *tables;

        uint32_t nimportedmems;
        uint32_t nmems;
        struct limits *mems;

        uint32_t nimportedglobals;
        uint32_t nglobals;
        struct global *globals;

        uint32_t nelems;
        struct element *elems;

        uint32_t ndatas;
        struct data *datas;

        bool has_start;
        uint32_t start;

        uint32_t nimports;
        struct import *imports;

        uint32_t nexports;
        struct export *exports;

        const uint8_t *bin;
};

struct exec_context;
struct host_instance;

typedef int (*host_func_t)(struct exec_context *, struct host_instance *hi,
                           const struct functype *ft, const struct val *params,
                           struct val *results);

struct funcinst {
        bool is_host;
        union {
                struct {
                        struct instance *instance;
                        uint32_t funcidx;
                } wasm;
                struct {
                        struct host_instance *instance;
                        struct functype *type;
                        host_func_t func;
                } host;
        } u;
};

struct meminst {
        uint8_t *data;
        uint32_t size_in_pages;
        uint32_t allocated;
        const struct limits *type;
};

struct globalinst {
        struct val val;
        const struct globaltype *type;
};

struct tableinst {
        struct val *vals;
        uint32_t size;
        const struct tabletype *type;
};

struct instance {
        struct module *module;
        VEC(, struct funcinst *) funcs;
        VEC(, struct meminst *) mems;
        VEC(, struct tableinst *) tables;
        VEC(, struct globalinst *) globals;

        struct bitmap data_dropped;
        struct bitmap elem_dropped;
};

struct import_object_entry {
        const struct name *module_name;
        const struct name *name;
        enum importtype type;
        union {
                struct funcinst *func;
                struct meminst *mem;
                struct tableinst *table;
                struct globalinst *global;
        } u;
};

struct import_object {
        size_t nentries;
        struct import_object_entry *entries;
        void (*dtor)(struct import_object *im);
        void *dtor_arg;
        struct import_object *next;
};

bool is_numtype(enum valtype vt);
bool is_vectype(enum valtype vt);
bool is_reftype(enum valtype vt);
bool is_valtype(enum valtype vt);
int compare_resulttype(const struct resulttype *a, const struct resulttype *b);
int compare_functype(const struct functype *a, const struct functype *b);
int compare_name(const struct name *a, const struct name *b);

const struct import *module_find_import(const struct module *m,
                                        enum importtype type, uint32_t idx);
const struct functype *module_functype(const struct module *m, uint32_t idx);
const struct limits *module_memtype(const struct module *m, uint32_t idx);
const struct tabletype *module_tabletype(const struct module *m, uint32_t idx);
const struct globaltype *module_globaltype(const struct module *m,
                                           uint32_t idx);

const struct functype *funcinst_functype(const struct funcinst *fi);

int functype_from_string(const char *p, struct functype **resultp);
void functype_free(struct functype *ft);

void clear_functype(struct functype *ft);
void clear_resulttype(struct resulttype *ft);

void set_name_cstr(struct name *name, char *cstr);
void clear_name(struct name *name);

#endif /* defined(_TYPE_H) */
