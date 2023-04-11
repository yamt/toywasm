struct cluster;

int suspend_check_interrupt(struct cluster *c);
void suspend_parked(struct cluster *c);
void suspend_threads(struct cluster *c);
void resume_threads(struct cluster *c);
