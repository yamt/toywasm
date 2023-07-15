#if !defined(_TYPE_H)
#define _TYPE_H

#include <stdbool.h>
#include <stdint.h>

#include "toywasm_config.h"

#include "bitmap.h"
#include "cell.h"
#include "lock.h"
#include "platform.h"
#include "vec.h"

#define WASM_MAGIC 0x6d736100

#define WASM_PAGE_SIZE 65536
#define WASM_MAX_PAGES 65536

struct jump {
        uint32_t pc;
        uint32_t targetpc;
};

struct type_annotation {
        uint32_t pc;
        uint32_t size;
};

struct type_annotations {
        uint32_t default_size;
        uint32_t ntypes;
        struct type_annotation *types;
};

/* hints for execution */
struct expr_exec_info {
        uint32_t njumps;
        struct jump *jumps;

        uint32_t maxlabels; /* max labels (including the implicit one) */
        uint32_t maxcells;  /* max cells on stack */

#if defined(TOYWASM_USE_SMALL_CELLS)
        /*
         * annotations for value-polymorphic instructions
         */
        struct type_annotations type_annotations;
#endif
};

/*
 * REVISIT: const exprs don't really need expr_exec_info.
 */
struct expr {
        const uint8_t *start;
#if defined(TOYWASM_ENABLE_WRITER)
        const uint8_t *end;
#endif

        struct expr_exec_info ei;
};

struct localcellidx {
        /*
         * This structure is used by TOYWASM_USE_RESULTTYPE_CELLIDX and
         * TOYWASM_USE_LOCALTYPE_CELLIDX to provide O(1) access to locals.
         *
         * Note: 16-bit offsets are used to save memory consumption.
         * If we use 32-bit here, it can spoil TOYWASM_USE_SMALL_CELLS
         * too badly.
         * Note: if a function has too many locals which can not be
         * represented with 16-bit, we simply give up using this structure
         * for the function and fallback to the slower method.
         *
         * The use of 16-bit offsets here imposes an implementation limit
         * on the number of locals which can use this structure.
         * While there seems to be no such a limit in the spec itself,
         * it seems that it's common to have similar limits to ease
         * implementations. Many of them are even hard limits without
         * any fallback. For examples,
         *
         * wasm-micro-runtime: UINT16_MAX cells
         * https://github.com/bytecodealliance/wasm-micro-runtime/blob/b5eea934cfaef5208a7bb4c9813699697d352fe1/core/iwasm/interpreter/wasm_loader.c#L1990
         *
         * wasmparser: MAX_WASM_FUNCTION_LOCALS = 50000
         * https://github.com/bytecodealliance/wasm-tools/blob/5e8639a37260cdc1aab196f5ae5c7ae6427c214f/crates/wasmparser/src/limits.rs#L28
         */
        uint16_t *cellidxes;
};

struct resulttype {
        enum valtype *types;
        uint32_t ntypes;
        bool is_static;
#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
        struct localcellidx cellidx;
#endif
};

#define DEFINE_TYPES(QUAL, NAME, ...) QUAL enum valtype NAME[] = {__VA_ARGS__}

#if defined(TOYWASM_USE_RESULTTYPE_CELLIDX)
#define CELLIDX_NONE                                                          \
        .cellidx = {                                                          \
                NULL,                                                         \
        },
#else
#define CELLIDX_NONE
#endif

#define DEFINE_RESULTTYPE(QUAL, NAME, TYPES, NTYPES)                          \
        QUAL struct resulttype NAME = {.types = (void *)TYPES,                \
                                       .ntypes = NTYPES,                      \
                                       .is_static = true,                     \
                                       CELLIDX_NONE}

extern const struct resulttype g_empty_rt;
#define empty_rt ((struct resulttype *)&g_empty_rt)

struct functype {
        struct resulttype parameter;
        struct resulttype result;
};

struct funcref {
        const struct funcinst *func;
};

union v128 {
        /*
         * i8[0]   bit 0..7
         * i8[1]   bit 8..15
         *   :
         * i8[N]   bit N*8 .. (N+1)*8-1
         *   :
         * i8[15]  bit 120..127
         *
         * i16[0]  bit 0..15 (little endian)
         *
         * this union uses the same representation as linear memory
         *
         * see also: the comment about type-punning in endian.c
         */
        uint8_t i8[16];
        uint16_t i16[8];
        uint32_t i32[4];
        uint64_t i64[2];
        float f32[4];
        double f64[2];
};

_Static_assert(sizeof(float) == 4, "float");
_Static_assert(sizeof(double) == 8, "double");
_Static_assert(sizeof(union v128) == 16, "v128");

/*
 * a value on operand stack, locals, etc.
 *
 * This fixed-sized representation allows simpler code
 * while it isn't space-efficient.
 */
struct val {
        union {
                /*
                 * Note: v128 is in little endian.
                 * others are host endian.
                 */
                uint32_t i32;
                uint64_t i64;
                float f32;
                double f64;
#if defined(TOYWASM_ENABLE_WASM_SIMD)
                union v128 v128;
#endif
                struct funcref funcref;
                void *externref;
#if defined(TOYWASM_USE_SMALL_CELLS)
#if defined(TOYWASM_ENABLE_WASM_SIMD)
                struct cell cells[4];
#else
                struct cell cells[2];
#endif
#else
                struct cell cells[1];
#endif
        } u;
};
#if defined(TOYWASM_ENABLE_WASM_SIMD)
_Static_assert(sizeof(struct val) == 16, "struct val");
#else
_Static_assert(sizeof(struct val) == 8, "struct val");
#endif

struct localchunk {
        enum valtype type;
        uint32_t n;
};

struct localtype {
        uint32_t nlocals;
        uint32_t nlocalchunks;
        struct localchunk *localchunks;
#if defined(TOYWASM_USE_LOCALTYPE_CELLIDX)
        struct localcellidx cellidx;
#endif
};

struct func {
        struct localtype localtype;
        struct expr e;
};

enum element_mode {
        ELEM_MODE_ACTIVE,
        ELEM_MODE_PASSIVE,
        ELEM_MODE_DECLARATIVE,
};

struct element {
        struct expr *init_exprs;
        uint32_t *funcs;
        uint32_t init_size; /* entries in init_exprs or funcs */
        enum valtype type;
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

/*
 * 0x02: shared (threads proposal)
 * 0x04: 64-bit (memory64 proposal)
 */
#define MEMTYPE_FLAG_SHARED 0x02
#define MEMTYPE_FLAG_64 0x04

struct memtype {
        struct limits lim;
        uint8_t flags; /* MEMTYPE_FLAGS_xxx */
};

struct tabletype {
        enum valtype et;
        struct limits lim;
};

enum externtype {
        EXTERNTYPE_FUNC = 0x00,
        EXTERNTYPE_TABLE = 0x01,
        EXTERNTYPE_MEMORY = 0x02,
        EXTERNTYPE_GLOBAL = 0x03,
};

struct importdesc {
        enum externtype type;
        union {
                uint32_t typeidx;
                struct memtype memtype;
                struct tabletype tabletype;
                struct globaltype globaltype;
        } u;
};

struct exportdesc {
        enum externtype type;
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

enum section_id {
        SECTION_ID_custom = 0,
        SECTION_ID_type,
        SECTION_ID_import,
        SECTION_ID_function,
        SECTION_ID_table,
        SECTION_ID_memory,
        SECTION_ID_global,
        SECTION_ID_export,
        SECTION_ID_start,
        SECTION_ID_element,
        SECTION_ID_code,
        SECTION_ID_data,
        SECTION_ID_datacount,
};

/*
 * https://webassembly.github.io/spec/core/syntax/modules.html#indices
 * > The index space for functions, tables, memories and globals
 * > includes respective imports declared in the same module.
 * > The indices of these imports precede the indices of other
 * > definitions in the same index space.
 */

/*
 * struct module represents a loaded module.
 *
 * It has referecences to the underlying module bytecode in a few places:
 *
 * - names
 * - data segments
 * - exprs
 * - module->bin (currently only used to calculate "pc")
 *
 * This structure and all referenced child structures are read-only
 * until module_destroy().
 * Thus it can be safely shared among threads without any serializations.
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
        struct memtype *mems;

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

struct cell;
typedef int (*host_func_t)(struct exec_context *, struct host_instance *hi,
                           const struct functype *ft,
                           const struct cell *params, struct cell *results);

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
        /* Note: memory_getptr2 reads size_in_pages w/o locks */
        _Atomic uint32_t size_in_pages; /* overrides type->min */
        size_t allocated;
        const struct memtype *type;

#if defined(TOYWASM_ENABLE_WASM_THREADS)
        /*
         * extra info for shared memory instance.
         * NULL for non-shared memory instance.
         */
        struct shared_meminst *shared;
#endif
};

struct globalinst {
        /*
         * While this can use cells instead of a val to save some memory,
         * I'm not sure if it's worth the cost (runtime and developer time)
         * because typical modules have only a few globals.
         */
        struct val val;
        const struct globaltype *type;
};

/*
 * REVISIT: as tables can only have reference types,
 * it isn't really worth to have the cell overhead.
 */
struct tableinst {
        struct cell *cells;
        uint32_t size; /* overrides type->min */
        const struct tabletype *type;
};

struct instance {
        const struct module *module;
        VEC(, struct funcinst *) funcs;
        VEC(, struct meminst *) mems;
        VEC(, struct tableinst *) tables;
        VEC(, struct globalinst *) globals;

        /*
         * Track which data/element has been dropped. It's unfortunate
         * that the functionality for space-saving actually just consumes
         * extra space for this implementation.
         */
        struct bitmap data_dropped;
        struct bitmap elem_dropped;
};

/*
 * import_object_entry represents a wasm external value which can
 * satisfy an import.
 *
 * https://webassembly.github.io/spec/core/exec/runtime.html#external-values
 */
struct import_object_entry {
        const struct name *module_name;
        const struct name *name;
        enum externtype type;
        union {
                struct funcinst *func;
                struct meminst *mem;
                struct tableinst *table;
                struct globalinst *global;
        } u;
};

/*
 * instance_create() takes a list of import_object, chained together
 * with the "next" member.
 * a chained list of import_object is a logical equivalent of
 * the importObject argument of the js-api:
 * https://webassembly.github.io/spec/js-api/index.html#instances
 *
 * instance_create() searches wasm external values in the list to
 * satisfy imports of the module.
 * If there are multiple matching entries, the first one is used.
 */
struct import_object {
        size_t nentries;
        struct import_object_entry *entries;
        void (*dtor)(struct import_object *im);
        void *dtor_arg;
        struct import_object *next; /* NULL for the last import_object */
};

bool is_numtype(enum valtype vt) __constfunc;
bool is_vectype(enum valtype vt) __constfunc;
bool is_reftype(enum valtype vt) __constfunc;
bool is_valtype(enum valtype vt) __constfunc;
int compare_resulttype(const struct resulttype *a, const struct resulttype *b);
int compare_functype(const struct functype *a, const struct functype *b);
int compare_name(const struct name *a, const struct name *b);

/*
 * note: given inst and idx, the following two are equivalent.
 * the former is usually a little cheaper.
 *
 *   module_functype(inst->module, idx)
 *   funcinst_functype(VEC_ELEM(inst->funcs, idx))
 */

const struct import *module_find_import(const struct module *m,
                                        enum externtype type, uint32_t idx);
const struct functype *module_functype(const struct module *m, uint32_t idx);
const struct memtype *module_memtype(const struct module *m, uint32_t idx);
const struct tabletype *module_tabletype(const struct module *m, uint32_t idx);
const struct globaltype *module_globaltype(const struct module *m,
                                           uint32_t idx);

const struct functype *funcinst_functype(const struct funcinst *fi);

int functype_from_string(const char *p, struct functype **resultp);
void functype_free(struct functype *ft);
int check_functype_with_string(struct module *m, uint32_t funcidx,
                               const char *sig);

void clear_functype(struct functype *ft);
void clear_resulttype(struct resulttype *rt);

void set_name_cstr(struct name *name, char *cstr);
void clear_name(struct name *name);

#endif /* defined(_TYPE_H) */
