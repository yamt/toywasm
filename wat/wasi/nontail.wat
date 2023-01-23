;; print hello

(module
  (func $fd_write (import "wasi_snapshot_preview1" "fd_write") (param i32 i32 i32 i32) (result i32))
  (func $proc_exit (import "wasi_snapshot_preview1" "proc_exit") (param i32))
  (func $call_fd_write (param i32 i32 i32 i32) (result i32)
    local.get 0
    local.get 1
    local.get 2
    local.get 3
    call $fd_write
  )
  (func (export "_start")
    i32.const 1
    i32.const 0
    i32.const 1
    i32.const 0
    call $call_fd_write
    i32.const 0
    i32.ne
    call $proc_exit
  )
  (memory (export "memory") 1)

  ;; iov_base = 0x100, iov_len = 6
  (data (i32.const 0) "\00\01\00\00\06\00\00\00")
  (data (i32.const 0x100) "hello\n")
)
