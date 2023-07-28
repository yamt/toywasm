#if 0
__attribute__((import_module("dyld")))
__attribute__((import_name("load_object"))) int
dyld_load_module(const char *name, size_t namelen, int flags, void *handlep);

__attribute__((import_module("dyld")))
__attribute__((import_name("resolve_symbol"))) int
dyld_resolve_symbol(void *handle, int symtype, const char *name,
                    size_t namelen, void **addrp);
#endif

/* "symtype" for dyld_resolve_symbol */
#define DYLD_SYMBOL_TYPE_FUNC 1
#define DYLD_SYMBOL_TYPE_MEMORY 2
