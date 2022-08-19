
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "endian.h"
#include "module_writer.h"
#include "type.h"
#include "util.h"

struct writer {
        void (*write)(struct writer *w, const void *p, size_t sz);
        FILE *fp;
        uint32_t size;
        int error;
};

static void
writer_write(struct writer *w, const void *p, size_t sz)
{
        w->write(w, p, sz);
}

static void
count(struct writer *w, const void *p, size_t sz)
{
        if (w->error != 0) {
                return;
        }
        if (UINT32_MAX - w->size < sz) {
                w->error = EOVERFLOW;
        }
        w->size += sz;
}

static void
counter_init(struct writer *w)
{
        memset(w, 0, sizeof(*w));
        w->write = count;
}

static void
write_to_file(struct writer *w, const void *p, size_t sz)
{
        if (w->error != 0) {
                return;
        }
        if (UINT32_MAX - w->size < sz) {
                w->error = EOVERFLOW;
        }
        size_t result = fwrite(p, sz, 1, w->fp);
        if (result != 1) {
                w->error = ferror(w->fp);
                if (w->error == 0) {
                        w->error = EIO;
                }
                return;
        }
        w->size += sz;
}

static void
writer_stdio_init(struct writer *w, FILE *fp)
{
        memset(w, 0, sizeof(*w));
        w->write = write_to_file;
        w->fp = fp;
}

static void
write_leb_u32(struct writer *w, uint32_t u)
{
        do {
                uint8_t u8 = u & 0x7f;
                u >>= 7;
                if (u != 0) {
                        u8 |= 0x80;
                }
                writer_write(w, &u8, sizeof(u8));
        } while (u != 0);
}

#define WRITE_U8(v)                                                           \
        do {                                                                  \
                uint8_t u8 = (v);                                             \
                writer_write(w, &u8, sizeof(u8));                             \
        } while (0)

#define WRITE_U32(v)                                                          \
        do {                                                                  \
                uint32_t u32 = host_to_le32(v);                               \
                writer_write(w, &u32, sizeof(u32));                           \
        } while (0)

#define WRITE_LEB_U32(v) write_leb_u32(w, (v))

#define WRITE_BYTES(p, sz) writer_write(w, p, sz)

static void
write_section_header(struct writer *w, uint8_t id, uint32_t size)
{
        WRITE_U8(id);
        WRITE_LEB_U32(size);
}

static void
write_valtype(struct writer *w, enum valtype vt)
{
        assert(is_valtype(vt));
        WRITE_U8(vt);
}

static void
write_resulttype(struct writer *w, const struct resulttype *rt)
{
        WRITE_LEB_U32(rt->ntypes);
        uint32_t i;
        for (i = 0; i < rt->ntypes; i++) {
                write_valtype(w, rt->types[i]);
        }
}

static void
write_functype(struct writer *w, const struct functype *ft)
{
        WRITE_U8(0x60);
        write_resulttype(w, &ft->parameter);
        write_resulttype(w, &ft->result);
}

static void
write_name(struct writer *w, const struct name *name)
{
        WRITE_LEB_U32(name->nbytes);
        WRITE_BYTES(name->data, name->nbytes);
}

static void
write_limits(struct writer *w, const struct limits *lim)
{
        if (lim->max == UINT32_MAX) {
                WRITE_U8(0x00);
                WRITE_LEB_U32(lim->min);
        } else {
                WRITE_U8(0x01);
                WRITE_LEB_U32(lim->min);
                WRITE_LEB_U32(lim->max);
        }
}

static void
write_tabletype(struct writer *w, const struct tabletype *tt)
{
        write_valtype(w, tt->et);
        write_limits(w, &tt->lim);
}

static void
write_memtype(struct writer *w, const struct limits *lim)
{
        write_limits(w, lim);
}

static void
write_globaltype(struct writer *w, const struct globaltype *gt)
{
        write_valtype(w, gt->t);
        switch (gt->mut) {
        case GLOBAL_VAR:
                WRITE_U8(0x01);
                break;
        case GLOBAL_CONST:
                WRITE_U8(0x00);
                break;
        default:
                assert(0);
        }
}

static void
write_importdesc(struct writer *w, const struct importdesc *desc)
{
        WRITE_U8(desc->type);
        switch (desc->type) {
        case 0x00:
                WRITE_LEB_U32(desc->u.typeidx);
                break;
        case 0x01:
                write_tabletype(w, &desc->u.tabletype);
                break;
        case 0x02:
                write_memtype(w, &desc->u.memtype.lim);
                break;
        case 0x03:
                write_globaltype(w, &desc->u.globaltype);
                break;
        default:
                assert(0);
        }
}

static void
write_import(struct writer *w, const struct import *im)
{
        write_name(w, &im->module_name);
        write_name(w, &im->name);
        write_importdesc(w, &im->desc);
}

static void
write_expr(struct writer *w, const struct expr *e)
{
        WRITE_BYTES(e->start, e->end - e->start);
}

static void
write_global(struct writer *w, const struct global *g)
{
        write_globaltype(w, &g->type);
        write_expr(w, &g->init);
}

static void
write_exportdesc(struct writer *w, const struct exportdesc *desc)
{
        switch (desc->type) {
        case EXPORT_FUNC:
                WRITE_U8(0x00);
                break;
        case EXPORT_TABLE:
                WRITE_U8(0x01);
                break;
        case EXPORT_MEMORY:
                WRITE_U8(0x02);
                break;
        case EXPORT_GLOBAL:
                WRITE_U8(0x03);
                break;
        default:
                assert(0);
        }
        WRITE_LEB_U32(desc->idx);
}

static void
write_export(struct writer *w, const struct export *ex)
{
        write_name(w, &ex->name);
        write_exportdesc(w, &ex->desc);
}

static void
write_element(struct writer *w, const struct element *e)
{
        uint32_t type;
        if (e->funcs != NULL) {
                switch (e->mode) {
                case ELEM_MODE_ACTIVE: /* 0, 2 */
                        if (e->table != 0 || e->type != TYPE_FUNCREF) {
                                type = 0x02;
                        } else {
                                type = 0x00;
                        }
                        break;
                case ELEM_MODE_PASSIVE: /* 1 */
                        assert(e->type == TYPE_FUNCREF);
                        type = 0x01;
                        break;
                case ELEM_MODE_DECLARATIVE: /* 3 */
                        type = 0x03;
                        break;
                }
        } else {
                assert(e->init_exprs != NULL);
                /* 4, 5, 6, 7 */
                switch (e->mode) {
                case ELEM_MODE_ACTIVE: /* 4, 6 */
                        if (e->table != 0 || e->type != TYPE_FUNCREF) {
                                type = 0x06;
                        } else {
                                type = 0x04;
                        }
                        break;
                case ELEM_MODE_PASSIVE: /* 5 */
                        type = 0x05;
                        break;
                case ELEM_MODE_DECLARATIVE: /* 7 */
                        type = 0x07;
                        break;
                }
        }
        WRITE_LEB_U32(type);
        switch (type) {
        case 2:
        case 6:
                WRITE_LEB_U32(e->table);
                break;
        default:
                break;
        }
        switch (type) {
        case 0:
        case 2:
        case 4:
        case 6:
                write_expr(w, &e->offset);
                break;
        default:
                break;
        }
        uint8_t elemkind;
        switch (type) {
        case 1:
        case 2:
        case 3:
                switch (e->type) {
                case TYPE_FUNCREF:
                        elemkind = 0x00;
                        break;
                default:
                        elemkind = 0; /* just to appease a compiler warning */
                        assert(0);
                }
                WRITE_U8(elemkind);
                break;
        case 5:
        case 6:
        case 7:
                assert(is_reftype(e->type));
                WRITE_U8(e->type);
                break;
        default:
                break;
        }
        uint32_t i;
        switch (type) {
        case 0:
        case 1:
        case 2:
        case 3:
                WRITE_LEB_U32(e->init_size);
                for (i = 0; i < e->init_size; i++) {
                        WRITE_LEB_U32(e->funcs[i]);
                }
                break;
        case 4:
        case 5:
        case 6:
        case 7:
                WRITE_LEB_U32(e->init_size);
                for (i = 0; i < e->init_size; i++) {
                        write_expr(w, &e->init_exprs[i]);
                }
        default:
                assert(0);
        }
}

static void
write_locals(struct writer *w, const struct func *func)
{
        WRITE_LEB_U32(func->nlocalchunks);
        uint32_t i;
        for (i = 0; i < func->nlocalchunks; i++) {
                const struct localchunk *chunk = &func->localchunks[i];
                WRITE_LEB_U32(chunk->n);
                WRITE_U8(chunk->type);
        }
}

static void
write_code(struct writer *w, const struct func *func)
{
        write_locals(w, func);
        write_expr(w, &func->e);
}

static void
write_func(struct writer *w, const struct func *func)
{
        struct writer counter;
        counter_init(&counter);
        write_code(&counter, func);
        if (counter.error != 0) {
                w->error = counter.error;
                return;
        }
        WRITE_LEB_U32(counter.size);
        write_code(w, func);
}

static void
write_data(struct writer *w, const struct data *data)
{
        if (data->mode == DATA_MODE_PASSIVE) {
                WRITE_LEB_U32(0x01);
        } else {
                if (data->memory == 0) {
                        WRITE_LEB_U32(0x00);
                } else {
                        WRITE_LEB_U32(0x02);
                        WRITE_LEB_U32(data->memory);
                }
                write_expr(w, &data->offset);
        }
        WRITE_LEB_U32(data->init_size);
        WRITE_BYTES(data->init, data->init_size);
}

static void
write_type_section(struct writer *w, const struct module *m)
{
        if (m->ntypes == 0) {
                return;
        }
        WRITE_LEB_U32(m->ntypes);
        uint32_t i;
        for (i = 0; i < m->ntypes; i++) {
                write_functype(w, &m->types[i]);
        }
}

static void
write_import_section(struct writer *w, const struct module *m)
{
        if (m->nimports == 0) {
                return;
        }
        WRITE_LEB_U32(m->nimports);
        uint32_t i;
        for (i = 0; i < m->nimports; i++) {
                write_import(w, &m->imports[i]);
        }
}

static void
write_function_section(struct writer *w, const struct module *m)
{
        if (m->nfuncs == 0) {
                return;
        }
        WRITE_LEB_U32(m->nfuncs);
        uint32_t i;
        for (i = 0; i < m->nfuncs; i++) {
                WRITE_LEB_U32(m->functypeidxes[i]);
        }
}

static void
write_table_section(struct writer *w, const struct module *m)
{
        if (m->ntables == 0) {
                return;
        }
        WRITE_LEB_U32(m->ntables);
        uint32_t i;
        for (i = 0; i < m->ntables; i++) {
                write_tabletype(w, &m->tables[i]);
        }
}

static void
write_memory_section(struct writer *w, const struct module *m)
{
        if (m->nmems == 0) {
                return;
        }
        WRITE_LEB_U32(m->nmems);
        uint32_t i;
        for (i = 0; i < m->nmems; i++) {
                write_memtype(w, &m->mems[i]);
        }
}

static void
write_global_section(struct writer *w, const struct module *m)
{
        if (m->nglobals == 0) {
                return;
        }
        WRITE_LEB_U32(m->nglobals);
        uint32_t i;
        for (i = 0; i < m->nglobals; i++) {
                write_global(w, &m->globals[i]);
        }
}

static void
write_export_section(struct writer *w, const struct module *m)
{
        if (m->nexports == 0) {
                return;
        }
        WRITE_LEB_U32(m->nexports);
        uint32_t i;
        for (i = 0; i < m->nexports; i++) {
                write_export(w, &m->exports[i]);
        }
}

static void
write_start_section(struct writer *w, const struct module *m)
{
        if (!m->has_start) {
                return;
        }
        WRITE_LEB_U32(m->start);
}

static void
write_element_section(struct writer *w, const struct module *m)
{
        if (m->nelems == 0) {
                return;
        }
        WRITE_LEB_U32(m->nelems);
        uint32_t i;
        for (i = 0; i < m->nelems; i++) {
                write_element(w, &m->elems[i]);
        }
}

static void
write_datacount_section(struct writer *w, const struct module *m)
{
        if (m->ndatas == 0) {
                return;
        }
        WRITE_LEB_U32(m->ndatas);
}

static void
write_code_section(struct writer *w, const struct module *m)
{
        if (m->nfuncs == 0) {
                return;
        }
        WRITE_LEB_U32(m->nfuncs);
        uint32_t i;
        for (i = 0; i < m->nfuncs; i++) {
                write_func(w, &m->funcs[i]);
        }
}

static void
write_data_section(struct writer *w, const struct module *m)
{
        if (m->ndatas == 0) {
                return;
        }
        WRITE_LEB_U32(m->ndatas);
        uint32_t i;
        for (i = 0; i < m->ndatas; i++) {
                write_data(w, &m->datas[i]);
        }
}

static void
write_section(struct writer *w, uint8_t id,
              void (*fn)(struct writer *w, const struct module *m),
              const struct module *m)
{
        struct writer counter;
        counter_init(&counter);
        fn(&counter, m);
        if (counter.error != 0) {
                w->error = counter.error;
                return;
        }
        if (counter.size > 0) {
                write_section_header(w, id, counter.size);
                fn(w, m);
        }
}

#define SECTION(n)                                                            \
        {                                                                     \
                .id = SECTION_ID_##n, .write = write_##n##_section,           \
        }

const static struct section {
        uint8_t id;
        void (*write)(struct writer *w, const struct module *m);
} sections[] = {
        SECTION(type),      SECTION(import), SECTION(function),
        SECTION(table),     SECTION(memory), SECTION(global),
        SECTION(export),    SECTION(start),  SECTION(element),
        SECTION(datacount), SECTION(code),   SECTION(data),
};

static void
module_write1(struct writer *w, const struct module *m)
{
        WRITE_U32(WASM_MAGIC);
        WRITE_U32(1); /* version */
        uint32_t i;
        for (i = 0; i < ARRAYCOUNT(sections); i++) {
                const struct section *s = &sections[i];
                write_section(w, s->id, s->write, m);
        }
}

int
module_write(const char *filename, const struct module *m)
{
        FILE *fp = fopen(filename, "w");
        int ret;
        if (fp == NULL) {
                assert(errno != 0);
                return errno;
        }
        struct writer writer;
        struct writer *w = &writer;
        writer_stdio_init(w, fp);
        module_write1(w, m);
        ret = fclose(fp);
        if (ret != 0) {
                assert(errno != 0);
                return errno;
        }
        if (w->error != 0) {
                return w->error;
        }
        return 0;
}
