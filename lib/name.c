/*
 * https://webassembly.github.io/spec/core/appendix/custom.html#name-section
 */

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>

#include "context.h"
#include "decode.h"
#include "leb128.h"
#include "name.h"
#include "type.h"
#include "xlog.h"

struct naming {
        uint32_t idx;
        uint32_t offset;
};

/*
 * https://github.com/WebAssembly/extended-name-section/blob/main/proposals/extended-name-section/Overview.md
 * https://webassembly.github.io/gc/core/appendix/custom.html#subsections
 */
enum namekind {
        NAME_KIND_MODULE = 0,
        NAME_KIND_FUNC = 1,
        NAME_KIND_LOCAL = 2,
        NAME_KIND_LABEL = 3,  /* extended-name-section */
        NAME_KIND_TYPE = 4,   /* GC */
        NAME_KIND_TABLE = 5,  /* extended-name-section */
        NAME_KIND_MEMORY = 6, /* extended-name-section */
        NAME_KIND_GLOBAL = 7, /* extended-name-section */
        NAME_KIND_ELEM = 8,   /* extended-name-section */
        NAME_KIND_DATA = 9,   /* extended-name-section */
        NAME_KIND_FIELD = 10, /* GC */
        NAME_KIND_TAG = 11,   /* extended-name-section */
};

/* Make aliases here just because it isn't a PC */
#define ptr2offset(m, p) ptr2pc(m, p)
#define offset2ptr(m, o) pc2ptr(m, o)

static const char *unknown_name = "<unknown>";

#if defined(TOYWASM_ENABLE_WASM_NAME_SECTION)

struct read_naming_context {
        const struct module *module;
        uint32_t lastidx;
};

static int
read_naming(const uint8_t **pp, const uint8_t *ep, uint32_t idx,
            struct naming *naming, void *vp)
{
        struct read_naming_context *ctx = vp;
        const struct module *m = ctx->module;
        const uint8_t *p = *pp;
        int ret;
        ret = read_leb_u32(&p, ep, &naming->idx);
        if (ret != 0) {
                return ret;
        }
        if (idx > 0 && naming->idx <= ctx->lastidx) {
                /*
                 * Note: the sequence of naming in a name map is sorted by
                 * index
                 */
                xlog_trace("non-ascending naming idx");
                return EINVAL;
        }
        ctx->lastidx = naming->idx;
        /*
         * after validating the name, only record the offset.
         */
        const uint8_t *name_len_ptr = p;
        struct name name;
        ret = read_name(&p, ep, &name);
        if (ret != 0) {
                return ret;
        }
        naming->offset = ptr2offset(m, name_len_ptr);
        *pp = p;
        return 0;
}

/* https://github.com/WebAssembly/design/blob/main/BinaryEncoding.md#name-map
 */
static int
parse_namemap(struct nametable *table, struct namemap *map,
              const struct module *m, const uint8_t *p, const uint8_t *ep)
{
        struct read_naming_context ctx;
        int ret;

        ctx.module = m;
        map->entries = NULL;
        ret = read_vec_with_ctx(&table->mctx, &p, ep, sizeof(struct naming),
                                read_naming, NULL, &ctx, &map->nentries,
                                &map->entries);
        if (ret != 0) {
                goto fail;
        }
        ret = 0;
fail:
        return ret;
}

static int
parse_name_section(struct nametable *table, const struct module *m)
{
        const uint8_t *p = m->name_section_start;
        const uint8_t *ep = m->name_section_end;
        int prev = -1;
        int ret;

        /*
         * loop over subsections.
         */
        while (p < ep) {
                uint32_t name_type;
                uint32_t name_payload_len;
                ret = read_leb_u32(&p, ep, &name_type);
                if (ret != 0) {
                        goto fail;
                }
                if (name_type > 0x7f) { /* varuint7 */
                        ret = EINVAL;
                        goto fail;
                }
                /*
                 * from the spec:
                 * > Each subsection may occur at most once, and in order of
                 * > increasing id.
                 */
                if ((int)name_type <= (int)prev) {
                        ret = EINVAL;
                        goto fail;
                }
                ret = read_leb_u32(&p, ep, &name_payload_len);
                if (ret != 0) {
                        goto fail;
                }
                switch (name_type) {
                case NAME_KIND_MODULE:
                        xlog_trace("name section: found NAME_KIND_MODULE "
                                   "subsection");
                        ret = read_name(&p, ep, &table->module_name);
                        if (ret != 0) {
                                goto fail;
                        }
                        continue; /* read_name already consumed payload */
                case NAME_KIND_FUNC:
                        xlog_trace("name section: found NAME_KIND_FUNC "
                                   "subsection");
                        ret = parse_namemap(table, &table->funcs, m, p, ep);
                        if (ret != 0) {
                                goto fail;
                        }
                        break;
                case NAME_KIND_LOCAL:
                        /*
                         * REVISIT: is it worth implementing local names?
                         *
                         * wat2wasm with --debug-names produces local names.
                         * i'm not aware of other toolchains producing them.
                         * LLVM doesn't produce them.
                         */
                        /* fallthrough */
                default:
                        xlog_trace("name section: ignore unimplemented kind "
                                   "%" PRIu32,
                                   name_type);
                        break;
                }
                /* skip the payload */
                if (ep - p < name_payload_len) {
                        xlog_trace("too long name subsection %" PRIu32,
                                   name_type);
                        ret = EINVAL;
                        break;
                }
                p += name_payload_len;
        }
        ret = 0;
fail:
        return ret;
}

static void
namemap_init(struct namemap *map)
{
        map->nentries = 0;
        map->entries = NULL;
}

static void
namemap_clear(struct nametable *table, struct namemap *map)
{
        mem_free(&table->mctx, map->entries,
                 map->nentries * sizeof(*map->entries));
        map->nentries = 0;
        map->entries = NULL;
}

void
namemap_lookup(const struct module *m, const struct namemap *map,
               uint32_t funcidx, struct name *name)
{
        /* binary search */
        uint32_t left = 0;
        uint32_t right = map->nentries;
        while (left < right) {
                uint32_t mid = (left + right) / 2;
                const struct naming *naming = &map->entries[mid];
                if (naming->idx == funcidx) {
                        /*
                         * Note: the name pointed by the offset has
                         * already been validated by read_naming().
                         */
                        const uint32_t offset = naming->offset;
                        const uint8_t *p = m->bin + offset;
                        name->nbytes = read_leb_u32_nocheck(&p);
                        name->data = (const char *)p;
                        return;
                }
                if (naming->idx < funcidx) {
                        left = mid + 1;
                } else {
                        right = mid;
                }
        }
        set_name_cstr(name, unknown_name);
}
#endif /* defined(TOYWASM_ENABLE_WASM_NAME_SECTION) */

void
nametable_init(struct nametable *table)
{
#if defined(TOYWASM_ENABLE_WASM_NAME_SECTION)
        mem_context_init(&table->mctx);
        namemap_init(&table->funcs);
        table->module_name.data = NULL;
        table->module = NULL;
#endif
}

#if defined(TOYWASM_ENABLE_WASM_NAME_SECTION)
static void
nametable_add_module(struct nametable *table, const struct module *m)
{
        if (m->name_section_start == NULL) {
                /* the module doesn't have the name section */
                return;
        }
        if (table->module == m) {
                /* the module has already been added to the table */
                return;
        }
        if (table->module != NULL) {
                /*
                 * for simplicity, the current implementation keeps only
                 * a single module.
                 * remove the existing module before adding a new one.
                 */
                nametable_clear(table);
        }
        int ret = parse_name_section(table, m);
        if (ret != 0) {
                nametable_clear(table);
                return;
        }
        table->module = m;
}
#endif

void
nametable_clear(struct nametable *table)
{
#if defined(TOYWASM_ENABLE_WASM_NAME_SECTION)
        namemap_clear(table, &table->funcs);
        table->module_name.data = NULL;
        table->module = NULL;
        mem_context_clear(&table->mctx);
#endif
}

void
nametable_lookup_func(struct nametable *table, const struct module *m,
                      uint32_t funcidx, struct name *name)
{
#if defined(TOYWASM_ENABLE_WASM_NAME_SECTION)
        nametable_add_module(table, m);
        if (table->module == m) {
                namemap_lookup(table->module, &table->funcs, funcidx, name);
        } else {
                set_name_cstr(name, unknown_name);
        }
#else
        set_name_cstr(name, unknown_name);
#endif
}

void
nametable_lookup_module(struct nametable *table, const struct module *m,
                        struct name *name)
{
#if defined(TOYWASM_ENABLE_WASM_NAME_SECTION)
        nametable_add_module(table, m);
        if (table->module == m && table->module_name.data != NULL) {
                *name = table->module_name;
        } else {
                set_name_cstr(name, unknown_name);
        }
#else
        set_name_cstr(name, unknown_name);
#endif
}
