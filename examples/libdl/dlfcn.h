/*
 * a simple dlopen/dlsym implementation backed by toywasm dyld_dlfcn
 */

#if !defined(_DLFCN_H)
#define _DLFCN_H

#define RTLD_LAZY 0x1
#undef RTLD_NOW /* unimplemented */
#define RTLD_GLOBAL 0x4
#undef RTLD_LOCAL /* unimplemented */

void *dlopen(const char *name, int mode);
void *dlsym(void *h, const char *name);
const char *dlerror();
void dlclose(void *handle);

#endif /* !defined(_DLFCN_H) */
