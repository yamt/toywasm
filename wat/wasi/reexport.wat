;; test a direct invocation of a host function:
;;
;; wat2wasm reexport.wat
;; toywasm --print-stats --wasi --load reexport.wasm --invoke "fd_read 0 0 0 0"
(module
  (func $fd_read (export "fd_read") (import "wasi_snapshot_preview1" "fd_read") (param i32 i32 i32 i32) (result i32))
  (memory (export "memory") 1)
)
