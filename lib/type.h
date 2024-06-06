#if !defined(_TOYWASM_TYPE_H)
#define _TOYWASM_TYPE_H

#include <stdbool.h>
#include <stdint.h>

#include "toywasm_config.h"

#include "bitmap.h"
#include "cell.h"
#include "lock.h"
#include "platform.h"
#include "vec.h"

#define WASM_MAGIC 0x6d736100

#define WASM_PAGE_SHIFT 16
#define WASM_MAX_MEMORY_SIZE 0x100000000

/*
 * jump table. see doc/annotations.md
 */
struct jump {
        uint32_t pc;
        uint32_t targetpc;
};

/*
 * type annotations. see doc/annotations.md
 */
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

/*
 * local offset table. see doc/annotations.md
 */
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

#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
#if !defined(TOYWASM_USE_SMALL_CELLS)
#error TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING w/o TOYWASM_USE_SMALL_CELLS is not implemented
#endif
/*
 * Note: the type of exc->cells is taginst_functype(exc->tag)->parameter.
 */
struct wasm_exception {
        struct cell cells[TOYWASM_EXCEPTION_MAX_CELLS];
        const struct taginst *tag;
};
ctassert_offset(struct wasm_exception, cells, 0);
/*
 * a complex way to say (struct cell *)&exc->tag avoiding UBSAN complaints.
 */
#define exception_tag_ptr(exc)                                                \
        ((uint8_t *)(exc) + toywasm_offsetof(struct wasm_exception, tag))
#endif /* defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING) */

/*
 * calculate how many cells we need in struct val.
 */
#if defined(TOYWASM_USE_SMALL_CELLS)
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
#define EXNREF_NCELLS                                                         \
        HOWMANY(sizeof(struct wasm_exception), sizeof(struct cell))
#else
#define EXNREF_NCELLS 0
#endif
#define EXTERNREF_NCELLS HOWMANY(sizeof(void *), sizeof(struct cell))
#define NUMTYPE_NCELLS 2
#if defined(TOYWASM_ENABLE_WASM_SIMD)
#define VECTYPE_NCELLS 4
#else
#define VECTYPE_NCELLS 0
#endif
#define _MAX(a, b) ((a > b) ? a : b)
#define VAL_NCELLS                                                            \
        _MAX(_MAX(_MAX(EXNREF_NCELLS, EXTERNREF_NCELLS), NUMTYPE_NCELLS),     \
             VECTYPE_NCELLS)
#else /* defined(TOYWASM_USE_SMALL_CELLS) */
#define VAL_NCELLS 1
#endif /* defined(TOYWASM_USE_SMALL_CELLS) */

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
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
                /*
                 * Note: Because we don't have GC, we implement exnref as
                 * a copy-able type, rather than a reference to an object.
                 */
                struct wasm_exception exnref;
#endif
                struct cell cells[VAL_NCELLS];
        } u;
};
#if defined(TOYWASM_USE_SMALL_CELLS)
/*
 * Note: because the largest member of the union might not have the largest
 * alignment, the union can be a bit larger than what's calculated above.
 * in that case, the last 4 byte of the structure is just an unused padding.
 */
_Static_assert(sizeof(struct val) == VAL_NCELLS * 4 ||
                       sizeof(struct val) == (VAL_NCELLS + 1) * 4,
               "struct val");
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
        /*
         * Note: this implementation uses max=UINT32_MAX to mean "no max".
         * It should be ok because the max possible table size (and memory
         * size in pages with custom-page-sizes) is 2^32-1. (== UINT32_MAX)
         *
         * cf. https://github.com/WebAssembly/spec/issues/1752
         */
        uint32_t max;
};

/*
 * MEMTYPE_FLAG_xxx is encoded in limits.
 *
 * https://webassembly.github.io/spec/core/binary/types.html#limits
 * https://github.com/WebAssembly/threads/blob/main/proposals/threads/Overview.md#spec-changes
 * https://github.com/WebAssembly/memory64/blob/main/proposals/memory64/Overview.md#binary-format
 * https://github.com/WebAssembly/custom-page-sizes/blob/main/proposals/custom-page-sizes/Overview.md#binary-encoding
 *
 * (0x01: has max)
 * 0x02: shared (threads proposal)
 * 0x04: 64-bit (memory64 proposal)
 * 0x08: has custom page size (custom-page-sizes proposal)
 */
#define MEMTYPE_FLAG_SHARED 0x02
#define MEMTYPE_FLAG_64 0x04
#define MEMTYPE_FLAG_CUSTOM_PAGE_SIZE 0x08

struct memtype {
        struct limits lim;
        uint8_t flags; /* MEMTYPE_FLAGS_xxx */
#if defined(TOYWASM_ENABLE_WASM_CUSTOM_PAGE_SIZES)
        uint8_t page_shift;
#endif
};

struct tabletype {
        enum valtype et;
        struct limits lim;
};

struct tagtype {
        /* at this point, there is only one type, TAG_TYPE_exception */
        uint32_t typeidx; /* func type index */
};

enum externtype {
        EXTERNTYPE_FUNC = 0x00,
        EXTERNTYPE_TABLE = 0x01,
        EXTERNTYPE_MEMORY = 0x02,
        EXTERNTYPE_GLOBAL = 0x03,
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        EXTERNTYPE_TAG = 0x04,
#endif
};

struct importdesc {
        enum externtype type;
        union {
                uint32_t typeidx;
                struct memtype memtype;
                struct tabletype tabletype;
                struct globaltype globaltype;
                struct tagtype tagtype;
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

/*
 * usage: printf("%.*s", CSTR(name));
 *
 * Note: This is a broken hack. Only use this for diagnostic purposes.
 * The names in wasm are in utf-8. To feed them to printf %s, we need to
 * convert them to the charset/encoding of the current C locale. Also, the
 * names in wasm can contain NUL. Maybe it's simpler to escape non-PCS
 * characters.
 */
#define CSTR(n) (int)(n)->nbytes, (n)->data

#define NAME_FROM_CSTR_LITERAL(C)                                             \
        {                                                                     \
                .nbytes = sizeof(C) - 1, .data = C,                           \
        }

#define NAME_FROM_CSTR(C)                                                     \
        {                                                                     \
                .nbytes = strlen(C), .data = C,                               \
        }

struct import {
        struct name module_name;
        struct name name;
        struct importdesc desc;
};

struct wasm_export {
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

enum tag_type {
        TAG_TYPE_exception = 0,
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
        SECTION_ID_tag,
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

#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        uint32_t nimportedtags;
        uint32_t ntags;
        struct tagtype *tags;
#endif

        uint32_t nelems;
        struct element *elems;

        uint32_t ndatas;
        struct data *datas;

        bool has_start;
        uint32_t start;

        uint32_t nimports;
        struct import *imports;

        uint32_t nexports;
        struct wasm_export *exports;

        const uint8_t *bin;

#if defined(TOYWASM_ENABLE_WASM_NAME_SECTION)
        /*
         * Unlike other sections, we don't parse the name section
         * when loading a module. Thus, a broken name section is not a
         * validation error.
         *
         * We provide some APIs (name.h) for embedders who want to use
         * the names in the section. We expect it's done only when
         * necessary. eg. When printing traces for debugging purposes.
         */
        const uint8_t *name_section_start;
        const uint8_t *name_section_end;
#endif
#if defined(TOYWASM_ENABLE_DYLD)
        struct dylink *dylink;
#endif
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

        /*
         * meminst->allocated is the number of bytes actually
         * allocated meminst->data. it can be smaller than size_in_bytes
         * in case of sub-page allocation. wasm page size (64KB) is a bit
         * too large for small use cases. the sub-page allocation is an
         * implementation detail which is not visible to the wasm instance.
         */
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

#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
/*
 * A note about tag identity
 *
 * Consider:
 * 1. A module (M) defines a tag (T) and exports it as E-T.
 * 2. The module is instantiated twice. (M1 and M2)
 *
 * Now, should M1:E-T and M2:E-T be considered to be different tags
 * when finding a matching catch block?
 * I guess it should. Because I think instances from a module
 * should behave the same as ones from two identical modules.
 */
struct taginst {
        const struct functype *type;
};
#endif

struct instance {
        const struct module *module;
        VEC(, struct funcinst *) funcs;
        VEC(, struct meminst *) mems;
        VEC(, struct tableinst *) tables;
        VEC(, struct globalinst *) globals;
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
        VEC(, struct taginst *) tags;
#endif

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
                struct taginst *tag;
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
#if defined(TOYWASM_SORT_EXPORTS)
        bool use_binary_search;
#endif
        size_t nentries;
        struct import_object_entry *entries;
        void (*dtor)(struct import_object *im);
        void *dtor_arg;
        struct import_object *next; /* NULL for the last import_object */
};

__BEGIN_EXTERN_C

extern const struct resulttype g_empty_rt;

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
const struct tagtype *module_tagtype(const struct module *m, uint32_t idx);
const struct functype *module_tagtype_functype(const struct module *m,
                                               const struct tagtype *tt);

const struct functype *funcinst_functype(const struct funcinst *fi);
const struct functype *taginst_functype(const struct taginst *ti);
const struct func *funcinst_func(const struct funcinst *fi);

int functype_from_string(const char *p, struct functype **resultp);
void functype_free(struct functype *ft);
int check_functype_with_string(const struct module *m, uint32_t funcidx,
                               const char *sig);
int functype_to_string(char **p, const struct functype *ft);
void functype_string_free(char *p);

void clear_functype(struct functype *ft);
void clear_resulttype(struct resulttype *rt);

void set_name_cstr(struct name *name, const char *cstr);
void clear_name(struct name *name);

uint32_t memtype_page_shift(const struct memtype *type);

__END_EXTERN_C

#endif /* defined(_TOYWASM_TYPE_H) */
