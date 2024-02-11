struct cluster;
struct exec_context;

/*
 * supend_check_interrupt: used by check_interrupt() to check
 * pending suspend requests.
 */
int suspend_check_interrupt(struct exec_context *ctx, const struct cluster *c);

/*
 * suspend_parked: called by each thread when it's suspended.
 *
 * called by instance_execute_handle_restart or its variations.
 * threads are blocked within this function until its execution is resumed.
 */
void suspend_parked(struct cluster *c);

/*
 * suspend_threads: request to suspend all other threads in the cluster.
 *
 * when suspend_threads returns, every other threads in the cluster
 * are either:
 * a) blocking in suspend_threads
 * b) or, rewinded the stack (by returning a restartable error) and
 *    blocking in suspend_parked.
 *
 * among threads calling suspend_threads, suspend_threads works as
 * a cluster-global mutex as well. that is, only one thread can execute
 * the code between suspend_threads and resume_threads in the same time.
 *
 * Note: this is a no-op for TOYWASM_USE_USER_SCHED=ON.
 */
void suspend_threads(struct cluster *c);

/*
 * resume_threads: request to undo suspend_threads.
 * should only be called after a successful call of suspend_threads().
 */
void resume_threads(struct cluster *c);
