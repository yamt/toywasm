#define WASI_API(a, b) HOST_FUNC_DECL(wasi_##a);
#define WASI_API2(a, b, c) HOST_FUNC_DECL(b);

#include "wasi_preview1.h"
#include "wasi_unstable.h"

#undef WASI_API
#undef WASI_API2
