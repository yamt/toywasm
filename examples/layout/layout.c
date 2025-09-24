#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <toywasm/expr_parser.h>
#include <toywasm/fileio.h>
#include <toywasm/leb128.h>
#include <toywasm/load_context.h>
#include <toywasm/mem.h>
#include <toywasm/module.h>
#include <toywasm/name.h>
#include <toywasm/type.h>
#include <toywasm/xlog.h>

static int
try_to_decode_const_expr_u32(const struct expr *expr, uint32_t *resultp)
{
        /*
         * only parse the simplest case:
         *
         *   i32.const (0x41) <leb128>
         *   end (0x0b)
         */
        struct parse_expr_context pctx;
        const uint8_t *p = expr->start;
        const uint8_t *p1 = p;
        int ret = EINVAL;

        parse_expr_context_init(&pctx);
        parse_expr(&p, &pctx);
        if (p != NULL && *p1 == 0x41) {
                p1++;
                *resultp = read_leb_u32_nocheck(&p1);
                parse_expr(&p, &pctx);
                if (p == NULL) {
                        ret = 0;
                }
        }
        parse_expr_context_clear(&pctx);
        return ret;
}

enum meminfo_entry_type {
        met_data,
        met_global,
};

struct meminfo_entry {
        enum meminfo_entry_type me_type;
        struct name me_name;
        uint32_t me_offset;
        uint32_t me_idx;
        bool me_offset_unknown;
        union {
                struct {
                        uint32_t size;
                } u_data;
        } me_u;
};

struct meminfo {
        uint32_t mi_nentries;
        struct meminfo_entry *mi_entries;
};

static void *
xcalloc(size_t count, size_t sz)
{
        void *p = calloc(count, sz);
        if (p == NULL) {
                xlog_error("xcalloc");
                exit(1);
        }
        return p;
}

static void *
xrealloc(void *p, size_t sz)
{
        void *np = realloc(p, sz);
        if (np == NULL) {
                xlog_error("xrealloc");
                exit(1);
        }
        return np;
}

static struct meminfo_entry *
meminfo_extend(struct meminfo *mi)
{
        uint32_t nentries = mi->mi_nentries;
        mi->mi_nentries = nentries + 1;
        mi->mi_entries = xrealloc(mi->mi_entries,
                                  mi->mi_nentries * sizeof(*mi->mi_entries));
        return &mi->mi_entries[nentries];
}

static int
me_compare(const void *va, const void *vb)
{
        const struct meminfo_entry *a = va;
        const struct meminfo_entry *b = vb;
        uint32_t a_offset = a->me_offset_unknown ? 0 : a->me_offset;
        uint32_t b_offset = b->me_offset_unknown ? 0 : b->me_offset;
        if (a_offset < b_offset) {
                return -1;
        }
        if (a_offset > b_offset) {
                return 1;
        }
        uint32_t a_end = a_offset;
        uint32_t b_end = b_offset;
        if (a->me_type == met_data) {
                a_end += a->me_u.u_data.size;
        }
        if (b->me_type == met_data) {
                b_end += b->me_u.u_data.size;
        }
        if (a_end < b_end) {
                return -1;
        }
        if (a_end > b_end) {
                return 1;
        }
        return 0;
}

static void
meminfo_sort(struct meminfo *mi)
{
        qsort(mi->mi_entries, mi->mi_nentries, sizeof(*mi->mi_entries),
              me_compare);
}

static void
meminfo_print(const struct meminfo *mi)
{
        uint32_t i;
        for (i = 0; i < mi->mi_nentries; i++) {
                const struct meminfo_entry *me = &mi->mi_entries[i];
                if (me->me_type == met_global) {
                        if (!me->me_offset_unknown) {
                                printf("global [%3" PRIu32 "] %08" PRIx32
                                       "          %.*s\n",
                                       me->me_idx, me->me_offset,
                                       CSTR(&me->me_name));
                        }
                }
                if (me->me_type == met_data) {
                        if (me->me_offset_unknown) {
                                printf("data   [%3" PRIu32 "] size %08" PRIx32
                                       " %.*s\n",
                                       me->me_idx, me->me_u.u_data.size,
                                       CSTR(&me->me_name));
                        } else {
                                printf("data   [%3" PRIu32 "] %08" PRIx32
                                       "-%08" PRIx32 " %.*s\n",
                                       me->me_idx, me->me_offset,
                                       me->me_offset + me->me_u.u_data.size,
                                       CSTR(&me->me_name));
                        }
                }
        }
}

int
main(int argc, char **argv)
{
        struct mem_context mctx;
        mem_context_init(&mctx);
        if (argc != 2) {
                xlog_error("unexpected number of args");
                exit(2);
        }
        const char *filename = argv[1];

        struct module *m;
        int ret;
        uint8_t *p;
        size_t sz;
        ret = map_file(filename, (void **)&p, &sz);
        if (ret != 0) {
                xlog_error("map_file failed with %d", ret);
                exit(1);
        }
        struct load_context ctx;
        load_context_init(&ctx, &mctx);
        ret = module_create(&m, p, p + sz, &ctx);
        if (ret != 0) {
                xlog_error("module_load failed with %d: %s", ret,
                           report_getmessage(&ctx.report));
                exit(1);
        }
        load_context_clear(&ctx);

        struct nametable table;
        nametable_init(&table);

        uint32_t memidx;
        uint32_t ntotalmems = m->nimportedmems + m->nmems;
        struct meminfo *mis = xcalloc(ntotalmems, sizeof(*mis));
        uint32_t i;
        for (i = 0; i < m->ndatas; i++) {
                const struct data *data = &m->datas[i];
                if (data->mode != DATA_MODE_ACTIVE) {
                        continue;
                }
                struct meminfo *mi = &mis[data->memory];
                struct meminfo_entry *me = meminfo_extend(mi);
                nametable_lookup_data(&table, m, i, &me->me_name);
                ret = try_to_decode_const_expr_u32(&data->offset,
                                                   &me->me_offset);
                if (ret != 0) {
                        me->me_offset_unknown = true;
                }
                me->me_type = met_data;
                me->me_idx = i;
                me->me_u.u_data.size = data->init_size;
        }
        for (i = m->nimportedglobals; i < m->nimportedglobals + m->nglobals;
             i++) {
                const struct global *g = &m->globals[i];
                if (g->type.t != TYPE_i32) {
                        continue;
                }
                /* assume the first memory */
                if (ntotalmems == 0) {
                        continue;
                }
                struct meminfo *mi = &mis[0];
                struct meminfo_entry *me = meminfo_extend(mi);
                nametable_lookup_global(&table, m, i, &me->me_name);
                ret = try_to_decode_const_expr_u32(&g->init, &me->me_offset);
                if (ret != 0) {
                        me->me_offset_unknown = true;
                }
                me->me_type = met_global;
                me->me_idx = i;
        }

        for (memidx = 0; memidx < ntotalmems; memidx++) {
                const struct memtype *mt = module_memtype(m, memidx);
                const uint32_t page_shift = memtype_page_shift(mt);
                printf("memory [%" PRIu32 "] min %08" PRIx64 " max %08" PRIx64
                       "\n",
                       memidx, (uint64_t)mt->lim.min << page_shift,
                       (uint64_t)mt->lim.max << page_shift);
                struct meminfo *mi = &mis[memidx];
                meminfo_sort(mi);
                meminfo_print(mi);
        }

        nametable_clear(&table);
        module_destroy(&mctx, m);
        mem_context_clear(&mctx);

        exit(0);
}
