#include <stdbool.h>
#include <stdint.h>

#include "toywasm_config.h"

#include "cell.h"
#include "exec_context.h"
#include "platform.h"

struct localtype;
struct module;

enum ctrlframe_op {
        FRAME_OP_BLOCK = 0x02,
        FRAME_OP_LOOP = 0x03,
        FRAME_OP_IF = 0x04,
        FRAME_OP_ELSE = 0x05,

        FRAME_OP_END = 0x0b,

        /* pseudo op */
        FRAME_OP_EMPTY_ELSE = 0xfe,
        FRAME_OP_INVOKE = 0xff,
};

/* For funcframe.funcidx */
#define FUNCIDX_INVALID UINT32_MAX

/* for exec_stats */
#define STAT_INC(s) (s)++

/* use shorter interval for userland thread */
#if defined(TOYWASM_USE_USER_SCHED) || !defined(TOYWASM_PREALLOC_SHARED_MEMORY)
#define CHECK_INTERRUPT_INTERVAL_MS 50
#else
#define CHECK_INTERRUPT_INTERVAL_MS 300
#endif

struct context {
        struct exec_context *exec;
        struct validation_context *validation;
};

/*
 * This "pc" is intended to be compatible with
 * the offset shown by "wasm-objdump -d".
 *
 * It's also compatible with "pcOffset" in the JS conventions:
 * https://webassembly.github.io/spec/web-api/index.html#conventions
 *
 * Besides logging, we use it in a few places to save space as it's
 * smaller than host pointers on 64-bit archs.
 */
uint32_t ptr2pc(const struct module *m, const uint8_t *p) __purefunc;
const uint8_t *pc2ptr(const struct module *m, uint32_t pc) __purefunc;

int resulttype_alloc(uint32_t ntypes, const enum valtype *types,
                     struct resulttype **resultp);
void resulttype_free(struct resulttype *p);

/* execution */

int trap(struct exec_context *ctx, const char *fmt, ...)
        __attribute__((__format__(__printf__, 2, 3)));
int trap_with_id(struct exec_context *ctx, enum trapid id, const char *fmt,
                 ...) __attribute__((__format__(__printf__, 3, 4)));
int memory_getptr(struct exec_context *ctx, uint32_t memidx, uint32_t ptr,
                  uint32_t offset, uint32_t size, void **pp);
int memory_getptr2(struct exec_context *ctx, uint32_t memidx, uint32_t ptr,
                   uint32_t offset, uint32_t size, void **pp, bool *movedp);
struct toywasm_mutex;
int memory_atomic_getptr(struct exec_context *ctx, uint32_t memidx,
                         uint32_t ptr, uint32_t offset, uint32_t size,
                         void **pp, struct toywasm_mutex **lockp);
void memory_atomic_unlock(struct toywasm_mutex *lock);
int frame_enter(struct exec_context *ctx, struct instance *inst,
                uint32_t funcidx, const struct expr_exec_info *ei,
                const struct localtype *localtype,
                const struct resulttype *paramtype, uint32_t nresults,
                const struct cell *params);
void frame_clear(struct funcframe *frame);
void frame_exit(struct exec_context *ctx);
struct cell *frame_locals(const struct exec_context *ctx,
                          const struct funcframe *frame) __purefunc;

uint32_t find_type_annotation(struct exec_context *ectx, const uint8_t *p);
