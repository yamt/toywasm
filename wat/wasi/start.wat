;; try to use wasi api from wasm start function
;;
;; i don't consider this a legal wasi command.
;; anyway, runtimes should not crash at least.

(module
  (func $fd_write (import "wasi_snapshot_preview1" "fd_write") (param i32 i32 i32 i32) (result i32))
  (func $proc_exit (import "wasi_snapshot_preview1" "proc_exit") (param i32))
  (func $start
    i32.const 1
    i32.const 0
    i32.const 1
    i32.const 0
    call $fd_write
    i32.const 0
    i32.ne
    call $proc_exit
  )
  (func (export "_start")
    unreachable
  )
  (memory (export "memory") 1)

  ;; iov_base = 0x100, iov_len = 6
  (data (i32.const 0) "\00\01\00\00\06\00\00\00")
  (data (i32.const 0x100) "hello\n")
  (start $start)
)
