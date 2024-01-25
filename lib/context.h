#include <stdbool.h>
#include <stdint.h>

#include "toywasm_config.h"

#include "cell.h"
#include "platform.h"

struct localtype;
struct module;

enum ctrlframe_op {
        FRAME_OP_BLOCK = 0x02,
        FRAME_OP_LOOP = 0x03,
        FRAME_OP_IF = 0x04,
        FRAME_OP_ELSE = 0x05,

        FRAME_OP_END = 0x0b,

        FRAME_OP_TRY_TABLE = 0x1f,

        /* pseudo op */
        FRAME_OP_EMPTY_ELSE = 0xfe,
        FRAME_OP_INVOKE = 0xff,
};

enum try_handler_tag {
        CATCH = 0x00,
        CATCH_REF = 0x01,
        CATCH_ALL = 0x02,
        CATCH_ALL_REF = 0x03,
};

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
