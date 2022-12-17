#define WASI_HOST_FUNC(NAME, TYPE)                                            \
        {                                                                     \
                .module_name = NULL, .name = NAME_FROM_CSTR_LITERAL(#NAME),   \
                .type = TYPE, .func = wasi_##NAME,                            \
        }

#define WASI_HOST_FUNC_WITH_MODULE_NAME(MODULE_NAME, NAME, TYPE)              \
        {                                                                     \
                .module_name = &MODULE_NAME,                                  \
                .name = NAME_FROM_CSTR_LITERAL(#NAME), .type = TYPE,          \
                .func = wasi_##NAME,                                          \
        }

#if defined(TOYWASM_ENABLE_TRACING)
#define WASI_TRACE                                                            \
        do {                                                                  \
                xlog_trace("WASI: %s called", __func__);                      \
        } while (0)
#else
#define WASI_TRACE                                                            \
        do {                                                                  \
        } while (0)
#endif

uint32_t wasi_convert_errno(int host_errno);

extern const struct name wasi_snapshot_preview1;
