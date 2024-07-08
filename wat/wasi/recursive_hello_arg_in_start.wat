;; use wasi in the wasm start function.
;;
;; wasmer doesn't seem happy with this:
;;
;;  spacetanuki% wasmer recursive_hello_arg_in_start.wasm foo
;;  thread 'main' panicked at lib/wasix/src/state/env.rs:924:37:
;;  You must initialize the WasiEnv before using it and can not pass it between threads
;;  note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace
;;  spacetanuki% wasmer --version
;;  wasmer 4.2.8
;;  spacetanuki% 

(module
  (func $fd_write (import "wasi_snapshot_preview1" "fd_write") (param i32 i32 i32 i32) (result i32))
  (func $proc_exit (import "wasi_snapshot_preview1" "proc_exit") (param i32))
  (func $args_get (import "wasi_snapshot_preview1" "args_get") (param i32 i32) (result i32))
  (func $args_sizes_get (import "wasi_snapshot_preview1" "args_sizes_get") (param i32 i32) (result i32))
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
  (func $print_args
    i32.const 0x1000 ;; &argc
    i32.const 0x1004
    call $args_sizes_get
    if
      unreachable
    end

    i32.const 0x1004 ;; &argv
    i32.const 0x2000
    call $args_get
    if
      unreachable
    end

    ;; hello,
    i32.const 0x100
    call $putstrz

    i32.const 0x1000
    i32.load
    i32.const 2
    i32.ne
    if (result i32)
      ;; noname
      i32.const 0x300
    else
      ;; argv[1]
      i32.const 0x1004
      i32.load offset=0x4
    end
    call $putstrz

    ;; newline
    i32.const 0x200
    call $putstrz
  )
  (func (export "_start")
  )
  (memory (export "memory") 1)

  ;; iov_base = 0, iov_len = 1
  (data (i32.const 0) "\00\00\00\00\01\00\00\00")
  (data (i32.const 0x100) "hello, \00")
  (data (i32.const 0x200) "\n\00")
  (data (i32.const 0x300) "noname\00")
  (start $print_args)
)
