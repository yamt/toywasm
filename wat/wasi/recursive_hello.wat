;; print hello

(module
  (func $fd_write (import "wasi_snapshot_preview1" "fd_write") (param i32 i32 i32 i32) (result i32))
  (func $proc_exit (import "wasi_snapshot_preview1" "proc_exit") (param i32))
  (func $putstrz (param $p i32)
    (local $tmp i32)
    local.get $p
    i32.load8_u
    local.tee $tmp
    i32.eqz
    br_if 0

    i32.const 0
    local.get $p
    i32.store

    i32.const 1
    i32.const 0
    i32.const 1
    i32.const 0
    call $fd_write
    i32.const 0
    i32.ne
    if
      i32.const 1
      call $proc_exit
    end

    local.get $p
    i32.const 1
    i32.add
    call $putstrz
  )
  (func (export "_start")
    i32.const 0x100
    call $putstrz
  )
  (memory (export "memory") 1)

  ;; iov_base = 0, iov_len = 1
  (data (i32.const 0) "\00\00\00\00\01\00\00\00")
  (data (i32.const 0x100) "hello, recursive\n\00")
)
