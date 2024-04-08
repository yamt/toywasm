#include <stddef.h>
#include <stdint.h>

#include "toywasm_config.h"

struct context;
struct exec_context;
struct validation_context;
struct cell;

struct exec_instruction_desc {
        /*
         * fetch_exec is called after fetching the first byte of
         * the instrution. '*p' points to the second byte.
         * it fetches and decodes the rest of the instrution,
         * and then executes it.
         */
        int (*fetch_exec)(const uint8_t *p, struct cell *stack,
                          struct exec_context *ctx);
};

struct instruction_desc {
        const char *name;
        int (*process)(const uint8_t **pp, const uint8_t *ep,
                       struct context *ctx);
#if defined(TOYWASM_USE_SEPARATE_VALIDATE)
        int (*validate)(const uint8_t *p, const uint8_t *ep,
                        struct validation_context *vctx);
#endif
        const struct instruction_desc *next_table;
        unsigned int next_table_size;
        unsigned int flags;
};

#define INSN_FLAG_CONST 1
#if defined(TOYWASM_ENABLE_WASM_EXTENDED_CONST)
#define INSN_FLAG_EXTENDED_CONST INSN_FLAG_CONST
#else
#define INSN_FLAG_EXTENDED_CONST 0
#endif

extern const struct exec_instruction_desc exec_instructions[];

extern const struct instruction_desc instructions[];
extern const size_t instructions_size;
