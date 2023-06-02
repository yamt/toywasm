(module
  (func $fd_write (import "wasi_snapshot_preview1" "fd_write") (param i32 i32 i32 i32) (result i32))
  (func $proc_exit (import "wasi_snapshot_preview1" "proc_exit") (param i32))
  (func $format_i32 (export "format_i32") (param $p i32) (param $v i32)
    (local $shift i32)

    ;; shift = 32
    i32.const 32
    local.set $shift

    loop
      ;; shift -= 4
      local.get $shift
      i32.const 4
      i32.sub
      local.set $shift

      ;; *p = ((uint8_t *)0x200)[(v >> shift) & 0xf]
      local.get $p
      local.get $v
      local.get $shift
      i32.shr_u
      i32.const 0xf
      i32.and
      i32.load8_u offset=0x200
      i32.store8

      ;; p++
      local.get $p
      i32.const 1
      i32.add
      local.set $p

      ;; continue if shift != 0
      local.get $shift
      i32.eqz
      br_if 1
      br 0
    end
  )
  (func $print_i32 (export "print_i32") (param $v i32) (result i32)
    i32.const 0x100
    local.get $v
    call $format_i32

    i32.const 1
    i32.const 0
    i32.const 1
    i32.const 0
    call $fd_write
  )
  (func (export "_start")
    i32.const 0xdeadbeef
    call $print_i32
    i32.const 0
    i32.ne
    call $proc_exit
  )
  (memory (export "memory") 1)

  ;; iov_base = 0x100, iov_len = 9
  (data (i32.const 0) "\00\01\00\00\09\00\00\00")
  (data (i32.const 0x100) "--------\n")
  (data (i32.const 0x200) "0123456789abcdef")
)
