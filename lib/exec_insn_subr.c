#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bitmap.h"
#include "exec.h"
#include "leb128.h"
#include "mem.h"
#include "platform.h"
#include "restart.h"
#include "shared_memory_impl.h"
#include "suspend.h"
#include "timeutil.h"
#include "type.h"
#include "util.h"
#include "xlog.h"

int
vtrap(struct exec_context *ctx, enum trapid id, const char *fmt, va_list ap)
{
        assert(!ctx->trapped);
        ctx->trapped = true;
        ctx->trap.trapid = id;
        vreport(ctx->report, fmt, ap);
        xlog_trace("TRAP: %s", ctx->report->msg);
        return ETOYWASMTRAP;
}

int
trap_with_id(struct exec_context *ctx, enum trapid id, const char *fmt, ...)
{
        int ret;
        va_list ap;
        va_start(ap, fmt);
        ret = vtrap(ctx, id, fmt, ap);
        va_end(ap);
        return ret;
}

int
memory_getptr2(struct exec_context *ctx, uint32_t memidx, uint32_t ptr,
               uint32_t offset, uint32_t size, void **pp, bool *movedp)
{
        const struct instance *inst = ctx->instance;
        assert(memidx < inst->module->nmems + inst->module->nimportedmems);
        struct meminst *meminst = VEC_ELEM(inst->mems, memidx);
        assert(meminst->allocated <=
               (uint64_t)meminst->size_in_pages
                       << memtype_page_shift(meminst->type));
        if (__predict_false(offset > UINT32_MAX - ptr)) {
                /*
                 * i failed to find this in the spec.
                 * but some of spec tests seem to test this.
                 */
                goto do_trap;
        }
        uint32_t ea = ptr + offset;
        if (__predict_false(size == 0)) {
                /*
                 * a zero-length access still needs address check.
                 * this can be either from host functions or
                 * bulk instructions like memory.copy.
                 */
                const uint32_t page_shift = memtype_page_shift(meminst->type);
                if (ea > 0 &&
                    (ea - 1) >> page_shift >= meminst->size_in_pages) {
                        goto do_trap;
                }
                goto success;
        }
        if (size - 1 > UINT32_MAX - ea) {
                goto do_trap;
        }
        uint32_t last_byte = ea + (size - 1);
        if (__predict_false(last_byte >= meminst->allocated)) {
                const uint32_t page_shift = memtype_page_shift(meminst->type);
                uint32_t need_in_pages = (last_byte >> page_shift) + 1;
                if (need_in_pages > meminst->size_in_pages) {
                        int ret;
do_trap:
                        ret = trap_with_id(
                                ctx, TRAP_OUT_OF_BOUNDS_MEMORY_ACCESS,
                                "invalid memory access at %04" PRIx32
                                " %08" PRIx32 " + %08" PRIx32 ", size %" PRIu32
                                ", meminst size %" PRIu32
                                ", pagesize %" PRIu32,
                                memidx, ptr, offset, size,
                                meminst->size_in_pages,
                                1 << memtype_page_shift(meminst->type));
                        assert(ret != 0); /* appease clang-tidy */
                        return ret;
                }
                /*
                 * Note: shared memories do never come here because
                 * we handle their growth in memory_grow.
                 */
                assert((meminst->type->flags & MEMTYPE_FLAG_SHARED) == 0);
#if SIZE_MAX <= UINT32_MAX
                if (last_byte >= SIZE_MAX) {
                        goto do_trap;
                }
#endif
                size_t need = (size_t)last_byte + 1;
                assert(need > meminst->allocated);
                void *np = mem_extend(meminst->mctx, meminst->data,
                                      meminst->allocated, need);
                if (np == NULL) {
                        return ENOMEM;
                }
                meminst->data = np;
                xlog_trace_insn("extend memory %" PRIu32 " from %zu to %zu",
                                memidx, meminst->allocated, need);
                if (movedp != NULL) {
                        *movedp = true;
                }
                memset(meminst->data + meminst->allocated, 0,
                       need - meminst->allocated);
                meminst->allocated = need;
        }
success:
        xlog_trace_insn("memory access: at %04" PRIx32 " %08" PRIx32
                        " + %08" PRIx32 ", size %" PRIu32
                        ", meminst size %" PRIu32,
                        memidx, ptr, offset, size, meminst->size_in_pages);
        *pp = meminst->data + ea;
        return 0;
}

int
memory_getptr(struct exec_context *ctx, uint32_t memidx, uint32_t ptr,
              uint32_t offset, uint32_t size, void **pp)
{
        return memory_getptr2(ctx, memidx, ptr, offset, size, pp, NULL);
}

#if defined(TOYWASM_ENABLE_WASM_THREADS)
int
memory_atomic_getptr(struct exec_context *ctx, uint32_t memidx, uint32_t ptr,
                     uint32_t offset, uint32_t size, void **pp,
                     struct toywasm_mutex **lockp)
        NO_THREAD_SAFETY_ANALYSIS /* conditionl lock */
{
        const struct instance *inst = ctx->instance;
        struct meminst *meminst = VEC_ELEM(inst->mems, memidx);
        struct shared_meminst *shared = meminst->shared;
        struct toywasm_mutex *lock = NULL;
        if (shared != NULL && lockp != NULL) {
                lock = atomics_mutex_getptr(&shared->tab, ptr + offset);
                toywasm_mutex_lock(lock);
        }
        int ret;
        ret = memory_getptr(ctx, memidx, ptr, offset, size, pp);
        if (ret != 0) {
                goto fail;
        }
        if (((ptr + offset) % size) != 0) {
                ret = trap_with_id(ctx, TRAP_UNALIGNED_ATOMIC_OPERATION,
                                   "unaligned atomic");
                assert(ret != 0); /* appease clang-tidy */
                goto fail;
        }
        if (lockp != NULL) {
                *lockp = lock;
        }
        return 0;
fail:
        memory_atomic_unlock(lock);
        return ret;
}

void
memory_atomic_unlock(struct toywasm_mutex *lock)
        NO_THREAD_SAFETY_ANALYSIS /* conditionl lock */
{
        if (lock != NULL) {
                toywasm_mutex_unlock(lock);
        }
}
#endif

int
memory_init(struct exec_context *ectx, uint32_t memidx, uint32_t dataidx,
            uint32_t d, uint32_t s, uint32_t n)
{
        const struct instance *inst = ectx->instance;
        const struct module *m = inst->module;
        assert(memidx < m->nimportedmems + m->nmems);
        assert(dataidx < m->ndatas);
        int ret;
        bool dropped = bitmap_test(&inst->data_dropped, dataidx);
        const struct data *data = &m->datas[dataidx];
        if ((dropped && !(s == 0 && n == 0)) || s > data->init_size ||
            n > data->init_size - s) {
                ret = trap_with_id(
                        ectx, TRAP_OUT_OF_BOUNDS_DATA_ACCESS,
                        "out of bounds data access: dataidx %" PRIu32
                        ", dropped %u, init_size %" PRIu32 ", s %" PRIu32
                        ", n %" PRIu32,
                        dataidx, dropped, data->init_size, s, n);
                goto fail;
        }
        void *p;
        ret = memory_getptr(ectx, memidx, d, 0, n, &p);
        if (ret != 0) {
                goto fail;
        }
        memcpy(p, &data->init[s], n);
        ret = 0;
fail:
        return ret;
}

int
table_access(struct exec_context *ectx, uint32_t tableidx, uint32_t offset,
             uint32_t n)
{
        const struct instance *inst = ectx->instance;
        const struct module *m = inst->module;
        assert(tableidx < m->nimportedtables + m->ntables);
        const struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
        if (offset > t->size || n > t->size - offset) {
                return trap_with_id(
                        ectx, TRAP_OUT_OF_BOUNDS_TABLE_ACCESS,
                        "out of bounds table access: table %" PRIu32
                        ", size %" PRIu32 ", offset %" PRIu32 ", n %" PRIu32,
                        tableidx, t->size, offset, n);
        }
        return 0;
}

int
table_init(struct exec_context *ectx, uint32_t tableidx, uint32_t elemidx,
           uint32_t d, uint32_t s, uint32_t n)
{
        const struct instance *inst = ectx->instance;
        const struct module *m = inst->module;
        assert(tableidx < m->nimportedtables + m->ntables);
        assert(elemidx < m->nelems);
        int ret;
        bool dropped = bitmap_test(&inst->elem_dropped, elemidx);
        const struct element *elem = &m->elems[elemidx];
        if ((dropped && !(s == 0 && n == 0)) || s > elem->init_size ||
            n > elem->init_size - s) {
                ret = trap_with_id(ectx, TRAP_OUT_OF_BOUNDS_ELEMENT_ACCESS,
                                   "out of bounds element access: dataidx "
                                   "%" PRIu32
                                   ", dropped %u, init_size %" PRIu32
                                   ", s %" PRIu32 ", n %" PRIu32,
                                   elemidx, dropped, elem->init_size, s, n);
                goto fail;
        }
        ret = table_access(ectx, tableidx, d, n);
        if (ret != 0) {
                goto fail;
        }
        struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
        assert(t->type->et == elem->type);
        uint32_t csz = valtype_cellsize(t->type->et);
        uint32_t i;
        for (i = 0; i < n; i++) {
                struct val val;
                if (elem->funcs != NULL) {
                        val.u.funcref.func =
                                VEC_ELEM(inst->funcs, elem->funcs[s + i]);
                } else {
                        ret = exec_const_expr(&elem->init_exprs[s + i],
                                              elem->type, &val, ectx);
                        if (ret != 0) {
                                goto fail;
                        }
                }
                val_to_cells(&val, &t->cells[(d + i) * csz], csz);
                xlog_trace("table %" PRIu32 " offset %" PRIu32
                           " initialized to %016" PRIx64,
                           tableidx, d + i, val.u.i64);
        }
        ret = 0;
fail:
        return ret;
}

void
table_set(struct tableinst *tinst, uint32_t elemidx, const struct val *val)
{
        uint32_t csz = valtype_cellsize(tinst->type->et);
        val_to_cells(val, &tinst->cells[elemidx * csz], csz);
}

void
table_get(struct tableinst *tinst, uint32_t elemidx, struct val *val)
{
        uint32_t csz = valtype_cellsize(tinst->type->et);
        val_from_cells(val, &tinst->cells[elemidx * csz], csz);
}

int
table_get_func(struct exec_context *ectx, const struct tableinst *t,
               uint32_t i, const struct functype *ft,
               const struct funcinst **fip)
{
        assert(t->type->et == TYPE_FUNCREF);
        int ret;
        if (__predict_false(i >= t->size)) {
                ret = trap_with_id(
                        ectx, TRAP_CALL_INDIRECT_OUT_OF_BOUNDS_TABLE_ACCESS,
                        "call_indirect (table idx out of range) "
                        "%" PRIu32,
                        i);
                goto fail;
        }
        struct val val;
        uint32_t csz = valtype_cellsize(t->type->et);
        val_from_cells(&val, &t->cells[i * csz], csz);
        const struct funcinst *func = val.u.funcref.func;
        if (__predict_false(func == NULL)) {
                ret = trap_with_id(ectx, TRAP_CALL_INDIRECT_NULL_FUNCREF,
                                   "call_indirect (null funcref) %" PRIu32, i);
                goto fail;
        }
        const struct functype *actual_ft = funcinst_functype(func);
        if (__predict_false(compare_functype(ft, actual_ft))) {
                ret = trap_with_id(
                        ectx, TRAP_CALL_INDIRECT_FUNCTYPE_MISMATCH,
                        "call_indirect (functype mismatch) %" PRIu32, i);
                goto fail;
        }
        *fip = func;
        return 0;
fail:
        return ret;
}

int
table_grow(struct tableinst *t, const struct val *val, uint32_t n)
{
        if (n == 0) {
                return t->size;
        }
        if (UINT32_MAX - t->size < n || t->size + n > t->type->lim.max) {
                return (uint32_t)-1;
        }

        uint32_t newsize = t->size + n;
        uint32_t csz = valtype_cellsize(t->type->et);
        size_t newncells = (size_t)newsize * csz;
        size_t newbytes = newncells * sizeof(*t->cells);
        int ret;
        if (newbytes / sizeof(*t->cells) / csz != newsize) {
                ret = EOVERFLOW;
        } else {
                size_t oldncells = (size_t)t->size * csz;
                size_t oldbytes = oldncells * sizeof(*t->cells);
                assert(oldbytes / sizeof(*t->cells) / csz == t->size);
                assert(oldbytes < newbytes);
                void *np = mem_extend(t->mctx, t->cells, oldbytes, newbytes);
                if (np == NULL) {
                        ret = ENOMEM;
                } else {
                        t->cells = np;
                        ret = 0;
                }
        }
        if (ret != 0) {
                return (uint32_t)-1;
        }

        uint32_t i;
        for (i = t->size; i < newsize; i++) {
                val_to_cells(val, &t->cells[i * csz], csz);
        }
        uint32_t oldsize = t->size;
        t->size = newsize;
        return oldsize;
}

void
global_set(struct globalinst *ginst, const struct val *val)
{
        ginst->val = *val;
}

void
global_get(struct globalinst *ginst, struct val *val)
{
        *val = ginst->val;
}

static void
memory_lock(struct meminst *mi) NO_THREAD_SAFETY_ANALYSIS
{
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        struct shared_meminst *shared = mi->shared;
        if (shared != NULL) {
                toywasm_mutex_lock(&shared->lock);
        }
#endif
}

static void
memory_unlock(struct meminst *mi) NO_THREAD_SAFETY_ANALYSIS
{
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        struct shared_meminst *shared = mi->shared;
        if (shared != NULL) {
                toywasm_mutex_unlock(&shared->lock);
        }
#endif
}

static uint32_t
memory_grow_impl(struct exec_context *ctx, struct meminst *mi, uint32_t sz)
{
        const struct memtype *mt = mi->type;
        const struct limits *lim = &mt->lim;
        const uint32_t page_shift = memtype_page_shift(mt);
        /*
         * In case of non-shared memory, no serialization is necessary.
         * in that case, memory_lock is no-op.
         *
         * For shared memory,
         * - mi->size_in_bytes is updated only with memory_lock held.
         *   also, it's _Atomic to allow fetches w/o the lock held.
         * - actual memory accesses including load/store instructions and
         *   host functions will be suspended with suspend_threads.
         *   mi->allocated is protected with the same mechanism.
         */
        memory_lock(mi);
        uint32_t orig_size;
#if defined(TOYWASM_ENABLE_WASM_THREADS)
retry:
#endif
        orig_size = mi->size_in_pages;
        if (sz > UINT32_MAX - orig_size) {
                memory_unlock(mi);
                return (uint32_t)-1; /* fail */
        }
        uint32_t new_size = orig_size + sz;
        assert(lim->max <= WASM_MAX_MEMORY_SIZE >> page_shift);
        if (new_size > lim->max) {
                memory_unlock(mi);
                return (uint32_t)-1; /* fail */
        }
        xlog_trace("memory grow %" PRIu32 " -> %" PRIu32, mi->size_in_pages,
                   new_size);
        bool do_realloc = new_size != orig_size;
#if defined(TOYWASM_ENABLE_WASM_THREADS)
        const bool shared = mi->shared != NULL;
        if (shared) {
#if defined(TOYWASM_PREALLOC_SHARED_MEMORY)
                do_realloc = false;
#endif
        } else
#endif /* defined(TOYWASM_ENABLE_WASM_THREADS) */
                if (new_size < 4) {
                        /*
                         * Note: for small non-shared memories, we defer the
                         * actual reallocation to memory_getptr2. (mainly to
                         * allow sub-page usage.)
                         */
                        do_realloc = false;
                }

        if (do_realloc) {
#if defined(TOYWASM_ENABLE_WASM_THREADS)
                struct cluster *c = ctx != NULL ? ctx->cluster : NULL;
                if (shared && c != NULL) {
                        /*
                         * suspend all other threads to ensure that no one is
                         * accessing the shared memory.
                         *
                         * REVISIT: doing this on every memory.grow is a bit
                         * expensive.
                         * maybe we can mitigate it by alloctating a bit more
                         * than requested.
                         */
                        memory_unlock(mi);
                        suspend_threads(c);
                        memory_lock(mi);
                        if (mi->size_in_pages != orig_size) {
                                goto retry;
                        }
                        assert((mi->allocated % (1 << page_shift)) == 0);
                }
#endif /* defined(TOYWASM_ENABLE_WASM_THREADS) */
                assert(new_size > mi->allocated >> page_shift);
                int ret;
                size_t new_size_in_bytes = (size_t)new_size << page_shift;
                if (new_size_in_bytes >> page_shift != new_size) {
                        ret = EOVERFLOW;
                } else {
                        void *np = mem_extend_zero(mi->mctx, mi->data,
                                                   mi->allocated,
                                                   new_size_in_bytes);
                        if (np == NULL) {
                                ret = ENOMEM;
                        } else {
                                ret = 0;
                                mi->data = np;
                                assert(new_size_in_bytes > mi->allocated);
                                mi->allocated = new_size_in_bytes;
                        }
                }
#if defined(TOYWASM_ENABLE_WASM_THREADS)
                if (shared && c != NULL) {
                        resume_threads(c);
                }
#endif /* defined(TOYWASM_ENABLE_WASM_THREADS) */
                if (ret != 0) {
                        memory_unlock(mi);
                        xlog_trace("%s: realloc failed", __func__);
                        return (uint32_t)-1; /* fail */
                }
        }
        mi->size_in_pages = new_size;
        memory_unlock(mi);
        return orig_size; /* success */
}

uint32_t
memory_grow2(struct exec_context *ctx, uint32_t memidx, uint32_t sz)
{
        const struct instance *inst = ctx->instance;
        const struct module *m = inst->module;
        assert(memidx < m->nimportedmems + m->nmems);
        struct meminst *mi = VEC_ELEM(inst->mems, memidx);
        return memory_grow_impl(ctx, mi, sz);
}

uint32_t
memory_grow(struct meminst *mi, uint32_t sz)
{
        return memory_grow_impl(NULL, mi, sz);
}

#if defined(TOYWASM_ENABLE_WASM_THREADS)
int
memory_notify(struct exec_context *ctx, uint32_t memidx, uint32_t addr,
              uint32_t offset, uint32_t count, uint32_t *nwokenp)
{
        const struct instance *inst = ctx->instance;
        const struct module *m = inst->module;
        assert(memidx < m->nimportedmems + m->nmems);
        struct meminst *mi = VEC_ELEM(inst->mems, memidx);
        struct shared_meminst *shared = mi->shared;
        struct toywasm_mutex *lock;
        void *p;
        int ret;
#if defined(__GNUC__) && !defined(__clang__)
        lock = NULL;
#endif
        ret = memory_atomic_getptr(ctx, memidx, addr, offset, 4, &p, &lock);
        if (ret != 0) {
                return ret;
        }
        assert((lock == NULL) == (shared == NULL));
        uint32_t nwoken;
        if (shared == NULL) {
                /* non-shared memory. we never have waiters. */
                nwoken = 0;
        } else {
                nwoken = atomics_notify(&shared->tab, addr + offset, count);
        }
        memory_atomic_unlock(lock);
        *nwokenp = nwoken;
        return 0;
}

int
memory_wait(struct exec_context *ctx, uint32_t memidx, uint32_t addr,
            uint32_t offset, uint64_t expected, uint32_t *resultp,
            int64_t timeout_ns, bool is64)
{
        const struct instance *inst = ctx->instance;
        const struct module *m = inst->module;
        assert(memidx < m->nimportedmems + m->nmems);
        struct meminst *mi = VEC_ELEM(inst->mems, memidx);
        struct shared_meminst *shared = mi->shared;
        if (shared == NULL) {
                /* non-shared memory. */
                return trap_with_id(ctx, TRAP_ATOMIC_WAIT_ON_NON_SHARED_MEMORY,
                                    "wait on non-shared memory");
        }
        struct toywasm_mutex *lock = NULL;
        int ret;

        /*
         * Note: it's important to always consume restart_abstimeout.
         */
        const struct timespec *abstimeout = NULL;
        ret = restart_info_prealloc(ctx);
        if (ret != 0) {
                return ret;
        }
        struct restart_info *restart = &VEC_NEXTELEM(ctx->restarts);
        assert(restart->restart_type == RESTART_NONE ||
               restart->restart_type == RESTART_TIMER);
        if (restart->restart_type == RESTART_TIMER) {
                abstimeout = &restart->restart_u.timer.abstimeout;
                restart->restart_type = RESTART_NONE;
        } else if (timeout_ns >= 0) {
                ret = abstime_from_reltime_ns(
                        CLOCK_REALTIME, &restart->restart_u.timer.abstimeout,
                        timeout_ns);
                if (ret != 0) {
                        goto fail;
                }
                abstimeout = &restart->restart_u.timer.abstimeout;
        }
        assert(restart->restart_type == RESTART_NONE);
        const uint32_t sz = is64 ? 8 : 4;
        void *p;
        ret = memory_atomic_getptr(ctx, memidx, addr, offset, sz, &p, &lock);
        if (ret != 0) {
                return ret;
        }
        assert((lock == NULL) == (shared == NULL));
#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
retry:;
#endif
        uint64_t prev;
        if (is64) {
                prev = *(_Atomic uint64_t *)p;
        } else {
                prev = *(_Atomic uint32_t *)p;
        }
        xlog_trace("%s: addr=0x%" PRIx32 " offset=0x%" PRIx32
                   " actual=%" PRIu64 " expected %" PRIu64,
                   __func__, addr, offset, prev, expected);
        if (prev != expected) {
                *resultp = 1; /* not equal */
        } else {
#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
                *resultp = 2;
#else
                /*
                 * emulate the user-specified long (or even infinite) block
                 * by looping with a short interval because we should call
                 * check_interrupt frequently enough.
                 */
                ret = check_interrupt(ctx);
                if (ret != 0) {
                        goto fail;
                }
                struct timespec next_abstimeout;
                const int interval_ms = check_interrupt_interval_ms(ctx);
                ret = abstime_from_reltime_ms(CLOCK_REALTIME, &next_abstimeout,
                                              interval_ms);
                if (ret != 0) {
                        goto fail;
                }
                const struct timespec *tv;
                if (abstimeout != NULL &&
                    timespec_cmp(&next_abstimeout, abstimeout) >= 0) {
                        tv = abstimeout;
                        xlog_trace("%s: abs %ju.%09lu\n", __func__,
                                   (uintmax_t)tv->tv_sec, tv->tv_nsec);
                } else {
                        tv = &next_abstimeout;
                        xlog_trace("%s: next %ju.%09lu\n", __func__,
                                   (uintmax_t)tv->tv_sec, tv->tv_nsec);
                }
                ret = atomics_wait(&shared->tab, addr + offset, tv);
                if (ret == 0) {
                        *resultp = 0; /* ok */
                } else if (ret == ETIMEDOUT) {
                        if (tv != abstimeout) {
                                goto retry;
                        }
                        *resultp = 2; /* timed out */
                } else {
                        /*
                         * REVISIT: while atomics_wait can possibly fail with
                         * ENOMEM, we don't have a nice way to deal with it.
                         * probably we can preallocate the memory similarly
                         * to solaris turnstiles to avoid the dynamic memory
                         * allocation in atomics_wait. but i'm not sure if
                         * it's worth the effort at this point. for now,
                         * just return an error. (it would cause the
                         * interpreter loop fail.)
                         */
                        goto fail;
                }
#endif
        }
        ret = 0;
fail:
        if (IS_RESTARTABLE(ret)) {
                if (abstimeout != NULL) {
                        assert(abstimeout ==
                               &restart->restart_u.timer.abstimeout);
                        restart->restart_type = RESTART_TIMER;
                }
                STAT_INC(ctx, atomic_wait_restart);
        }
        memory_atomic_unlock(lock);
        if (ret == 0) {
                xlog_trace("%s: returning %d result %d", __func__, ret,
                           *resultp);
        } else {
                xlog_trace("%s: returning %d", __func__, ret);
        }
        return ret;
}
#endif

/*
 * invoke: call a function.
 *
 * Note: the "finst" here can be a host function.
 */
int
invoke(struct funcinst *finst, const struct resulttype *paramtype,
       const struct resulttype *resulttype, struct exec_context *ctx)
{
        const struct functype *ft = funcinst_functype(finst);

        /*
         * Optional type check.
         */
        assert((paramtype == NULL) == (resulttype == NULL));
        if (paramtype != NULL) {
                if (compare_resulttype(paramtype, &ft->parameter) != 0 ||
                    compare_resulttype(resulttype, &ft->result) != 0) {
                        return EINVAL;
                }
        }

        /* Sanity check */
        assert(ctx->stack.lsize >= resulttype_cellsize(&ft->parameter));

        /*
         * Set up the context as if it was a restart of a "call" instruction.
         */

        ctx->event_u.call.func = finst;
        ctx->event = EXEC_EVENT_CALL;

        /*
         * and then "restart" the context execution.
         */
        return ETOYWASMRESTART;
}

void
data_drop(struct exec_context *ectx, uint32_t dataidx)
{
        struct instance *inst = ectx->instance;
        const struct module *m = inst->module;
        assert(dataidx < m->ndatas);
        bitmap_set(&inst->data_dropped, dataidx);
}

void
elem_drop(struct exec_context *ectx, uint32_t elemidx)
{
        struct instance *inst = ectx->instance;
        const struct module *m = inst->module;
        assert(elemidx < m->nelems);
        bitmap_set(&inst->elem_dropped, elemidx);
}
