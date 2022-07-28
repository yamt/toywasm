#include <stddef.h>
#include <stdint.h>

struct context;
struct exec_context;
struct val;

struct instruction_desc {
        const char *name;
        int (*process)(const uint8_t **pp, const uint8_t *ep,
                       struct context *ctx);
#if defined(USE_SEPARATE_EXECUTE)
        int (*execute)(const uint8_t *p, struct val *stack,
                       struct exec_context *ctx);
#endif
        const struct instruction_desc *next_table;
        unsigned int next_table_size;
        unsigned int flags;
};

#define INSN_FLAG_CONST 1

extern const struct instruction_desc instructions[];
extern const size_t instructions_size;
