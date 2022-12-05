#include <stddef.h>
#include <stdint.h>

struct context;
struct exec_context;
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
        const struct instruction_desc *next_table;
        unsigned int next_table_size;
        unsigned int flags;
};

#define INSN_FLAG_CONST 1

extern const struct exec_instruction_desc exec_instructions[];

extern const struct instruction_desc instructions[];
extern const size_t instructions_size;
