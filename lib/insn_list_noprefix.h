#include "insn_list_base.h"
#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
#include "insn_list_tailcall.h"
#endif /* defined(TOYWASM_ENABLE_WASM_TAILCALL) */
#if defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
#include "insn_list_eh.h"
#endif /* defined(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING) */
