struct import_object;
struct mem_context;

int import_object_create_for_log_execution(struct mem_context *mctx,
                                           void *inst,
                                           struct import_object **impp);
