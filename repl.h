#include <stdbool.h>

extern const char *g_repl_prompt;
extern struct repl_state *g_repl_state;
extern bool g_repl_use_jump_table;

int repl(void);
void repl_reset(struct repl_state *state);
int repl_load(struct repl_state *state, const char *filename);
int repl_register(struct repl_state *state, const char *module_name);
int repl_invoke(struct repl_state *state, const char *cmd, bool print_result);

int repl_load_wasi(struct repl_state *state);
int repl_set_wasi_args(struct repl_state *state, int argc, char *const *argv);
