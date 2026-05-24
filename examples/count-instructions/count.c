#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <toywasm/expr_parser.h>
#include <toywasm/fileio.h>
#include <toywasm/load_context.h>
#include <toywasm/mem.h>
#include <toywasm/module.h>
#include <toywasm/name.h>
#include <toywasm/type.h>
#include <toywasm/xlog.h>

struct funcinfo {
        uint32_t idx;
        uint32_t ninstructions;
        uint32_t nbytes;
};

static void
count_instructions_in_exprs(const struct expr *expr, uint32_t *ninstp,
                            uint32_t *nbytesp)
{
        uint32_t ninst = 0;
        uint32_t nbytes = 0;
        struct parse_expr_context pctx;
        parse_expr_context_init(&pctx);
        const uint8_t *p = expr->start;
        const uint8_t *p1;
        do {
                p1 = p;
                parse_expr(&p, &pctx);
                ninst++;
        } while (p != NULL);
        parse_expr_context_clear(&pctx);
        /* +1 for the "end" instruction */
        nbytes += p1 + 1 - expr->start;
        *ninstp = ninst;
        *nbytesp = nbytes;
}

static int
cmp_ninstructions_rev(const void *a, const void *b)
{
        const struct funcinfo *fia = a;
        const struct funcinfo *fib = b;
        if (fia->ninstructions < fib->ninstructions) {
                return 1;
        }
        if (fia->ninstructions > fib->ninstructions) {
                return -1;
        }
        return 0;
}

static int
cmp_nbytes_rev(const void *a, const void *b)
{
        const struct funcinfo *fia = a;
        const struct funcinfo *fib = b;
        if (fia->nbytes < fib->nbytes) {
                return 1;
        }
        if (fia->nbytes > fib->nbytes) {
                return -1;
        }
        return 0;
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

        struct funcinfo *fi = calloc(m->nfuncs, sizeof(*fi));
        if (fi == NULL) {
                xlog_error("calloc failed");
                exit(1);
        }

        uint32_t total_ninst = 0;
        uint32_t total_nbytes = 0;
        uint32_t i;
        for (i = 0; i < m->nfuncs; i++) {
                uint32_t ninst;
                uint32_t nbytes;
                const struct func *func = &m->funcs[i];
                count_instructions_in_exprs(&func->e, &ninst, &nbytes);
                fi[i].idx = m->nimportedfuncs + i;
                fi[i].ninstructions = ninst;
                fi[i].nbytes = nbytes;
                total_ninst += ninst;
                total_nbytes += nbytes;
        }
        printf("%" PRIu32 " functions\n", m->nfuncs);
        printf("%" PRIu32 " instructions\n", total_ninst);
        printf("%" PRIu32 " bytes\n", total_nbytes);
        if (total_ninst > 0) {
                printf("%g bytes / instructions\n",
                       (double)total_nbytes / total_ninst);
        }
        if (m->nfuncs > 0) {
                printf("%g instructions / functions\n",
                       (double)total_ninst / m->nfuncs);
                printf("%g bytes / functions\n",
                       (double)total_nbytes / m->nfuncs);

                uint32_t n = 5;
                if (n > m->nfuncs) {
                        n = m->nfuncs;
                }
                struct nametable table;
                nametable_init(&table);
                printf("\nlargest functions (in instructions):\n");
                qsort(fi, m->nfuncs, sizeof(*fi), cmp_ninstructions_rev);
                for (i = 0; i < n; i++) {
                        struct name func_name;
                        nametable_lookup_func(&table, m, fi[i].idx,
                                              &func_name);
                        printf("%8" PRIu32 " : func %" PRIu32 " (%.*s)\n",
                               fi[i].ninstructions, fi[i].idx,
                               CSTR(&func_name));
                }
                printf("\nlargest functions (in bytes):\n");
                qsort(fi, m->nfuncs, sizeof(*fi), cmp_nbytes_rev);
                for (i = 0; i < n; i++) {
                        struct name func_name;
                        nametable_lookup_func(&table, m, fi[i].idx,
                                              &func_name);
                        printf("%8" PRIu32 " : func %" PRIu32 " (%.*s)\n",
                               fi[i].nbytes, fi[i].idx, CSTR(&func_name));
                }
                nametable_clear(&table);
        }

        module_destroy(&mctx, m);
        mem_context_clear(&mctx);

        exit(0);
}
