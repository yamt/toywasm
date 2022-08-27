#include <stddef.h>
#include <stdint.h>

struct context;
struct exec_context;
struct cell;

struct exec_instruction_desc {
        int (*execute)(const uint8_t *p, struct cell *stack,
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
