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
int wasi_copyout(struct exec_context *ctx, const void *hostaddr,
                 uint32_t wasmaddr, size_t len);
int wasi_copyin(struct exec_context *ctx, void *hostaddr, uint32_t wasmaddr,
                size_t len);
