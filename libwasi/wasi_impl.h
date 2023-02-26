#define WASI_HOST_FUNC(NAME, TYPE)                                            \
        {                                                                     \
                .name = NAME_FROM_CSTR_LITERAL(#NAME), .type = TYPE,          \
                .func = wasi_##NAME,                                          \
        }

#define WASI_HOST_FUNC2(NAME, FUNC, TYPE)                                     \
        {                                                                     \
                .name = NAME_FROM_CSTR_LITERAL(NAME), .type = TYPE,           \
                .func = FUNC,                                                 \
        }

#if defined(TOYWASM_ENABLE_TRACING)
#define WASI_TRACE                                                            \
        do {                                                                  \
                xlog_trace("WASI: %s called", __func__);                      \
                host_func_dump_params(ft, params);                            \
        } while (0)
#else
#define WASI_TRACE                                                            \
        do {                                                                  \
        } while (0)
#endif

uint32_t wasi_convert_errno(int host_errno);

#define wasi_copyout(c, h, w, l) host_func_copyout(c, h, w, l)
#define wasi_copyin(c, h, w, l) host_func_copyin(c, h, w, l)
