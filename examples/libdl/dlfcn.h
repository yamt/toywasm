/*
 * a simple dlopen/dlsym implementation backed by toywasm dyld_dlfcn
 */

#if !defined(_DLFCN_H)
#define _DLFCN_H

#define RTLD_LAZY 0x0001
#define RTLD_NOW 0x0002
#define RTLD_GLOBAL 0x0100
#define RTLD_LOCAL 0

void *dlopen(const char *name, int mode);
void *dlsym(void *h, const char *name);
const char *dlerror();
void dlclose(void *handle);

#endif /* !defined(_DLFCN_H) */
