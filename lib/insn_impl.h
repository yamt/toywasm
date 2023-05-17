#include "insn_impl_base.h"
#include "insn_impl_control.h"
#include "insn_impl_fc.h"
#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
#include "insn_impl_tailcall.h"
#endif
#if defined(TOYWASM_ENABLE_WASM_SIMD)
#include "insn_impl_simd.h"
#endif
#if defined(TOYWASM_ENABLE_WASM_THREADS)
#include "insn_impl_threads.h"
#endif
