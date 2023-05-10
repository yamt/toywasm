;; https://github.com/WebAssembly/WASI/issues/505
;;
;; implementations disagree what should happens on out of range access
;; during a wasi call.
;;
;; wasm3, wasm-micro-runtime: "out of bounds memory access" trap
;; wasmer, wasmtime: exit with 21 (WASI_ERRNO_FAULT)

;; toywasm    trap  cf. https://github.com/yamt/toywasm/pull/2
;; wasm3      trap
;; wamr       trap
;; wasmtime   exit with 21 (WASI_ERRNO_FAULT)
;; old wasmer exit with 21 (WASI_ERRNO_FAULT)
;; wasmer     exit with 78 (memviolation, wasmer dialect of WASI)
;; wazero     exit with 21 (WASI_ERRNO_FAULT)
;; wasmi_cli  exit with 21 (WASI_ERRNO_FAULT)
;; wasmedge   exit with 21 (WASI_ERRNO_FAULT)

(module
  (func $fd_write (import "wasi_snapshot_preview1" "fd_write") (param i32 i32 i32 i32) (result i32))
  (func $proc_exit (import "wasi_snapshot_preview1" "proc_exit") (param i32))
  (func (export "_start")
     ;; iov->iov_base = 0xffffffff
     i32.const 0
     i32.const 0xfffffff0 ;; out of range address
     i32.store
     ;; iov->iov_len = 100
     i32.const 4
     i32.const 100
     i32.store

     i32.const 1 ;; fd = STDOUT
     i32.const 0 ;; iov_addr
     i32.const 1 ;; iov_count
     i32.const 8 ;; retp
     call $fd_write
     call $proc_exit
  )
  (memory (export "memory") 1)
)
