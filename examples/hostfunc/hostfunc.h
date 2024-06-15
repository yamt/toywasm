struct mem_context;
struct import_object;

int import_object_create_for_my_host_inst(struct mem_context *mctx, void *inst,
                                          struct import_object **impp);
