#include <stdbool.h>

#include "platform.h"

__BEGIN_EXTERN_C

struct exec_context;
int restart_info_prealloc(struct exec_context *);
void restart_info_clear(struct exec_context *ctx);
bool restart_info_is_none(struct exec_context *ctx);

__END_EXTERN_C
