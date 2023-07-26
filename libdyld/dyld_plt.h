int dyld_resolve_plt(struct exec_context *ectx, struct dyld_plt *plt);
int dyld_plt(struct exec_context *ctx, struct host_instance *hi,
             const struct functype *ft, const struct cell *params,
             struct cell *results);
