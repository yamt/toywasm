#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "exec.h"
#include "wasi_impl.h"

#include "wasi_hostfuncs.h"

int
wasi_proc_exit(struct exec_context *ctx, struct host_instance *hi,
               const struct functype *ft, const struct cell *params,
               struct cell *results)
{
        WASI_TRACE;
        struct wasi_instance *wasi = (void *)hi;
        HOST_FUNC_CONVERT_PARAMS(ft, params);
        uint32_t code = HOST_FUNC_PARAM(ft, params, 0, i32);
        /*
         * Note: While our embedder API (wasi_instance_exit_code) has
         * no problem to propagate a full 32-bit value, toywasm cli
         * can't represent full range with its exit code, especially
         * when it's running on a wasm runtime with restricted wasi
         * exit code.
         *
         * Note: wasmtime traps for exit code >= 126.
         *
         * Note: in preview1, only 0 has a defined meaning.
         * https://github.com/WebAssembly/WASI/blob/ddfe3d1dda5d1473f37ecebc552ae20ce5fd319a/legacy/preview1/witx/wasi_snapshot_preview1.witx#L460-L462
         * > An exit code of 0 indicates successful termination of
         * > the program. The meanings of other values is dependent on
         * > the environment.
         *
         * Note: A change to restrict wasi exit code to [0,126) has been
         * made to ephemeral. But it has never been a part of preview1.
         * https://github.com/WebAssembly/WASI/pull/235
         * https://github.com/WebAssembly/WASI/pull/510
         */
        toywasm_mutex_lock(&wasi->lock);
        wasi->exit_code = code;
        toywasm_mutex_unlock(&wasi->lock);
        HOST_FUNC_FREE_CONVERTED_PARAMS();
        return trap_with_id(ctx, TRAP_VOLUNTARY_EXIT,
                            "proc_exit with %" PRIu32, code);
}
