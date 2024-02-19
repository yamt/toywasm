struct path_info {
        char *hostpath;
        char *wasmpath;
};

#define PATH_INITIALIZER                                                      \
        {                                                                     \
                NULL, NULL,                                                   \
        }

struct exec_context;
struct wasi_instance;

void path_clear(struct path_info *pi);
int wasi_copyin_and_convert_path(struct exec_context *ctx,
                                 struct wasi_instance *wasi,
                                 uint32_t dirwasifd, uint32_t path,
                                 uint32_t pathlen, struct path_info *pi,
                                 int *usererrorp);
