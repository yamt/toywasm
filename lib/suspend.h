struct cluster;
struct exec_context;

int suspend_check_interrupt(struct exec_context *ctx, const struct cluster *c);
void suspend_parked(struct cluster *c);
void suspend_threads(struct cluster *c);
void resume_threads(struct cluster *c);
