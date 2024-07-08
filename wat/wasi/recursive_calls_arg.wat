;; just make recursive calls.
;; the number of recursive calls are specified via WASI cli argument.
;; the main motivation is to test --max-frames and --max-stack-cells.
;;
;; spacetanuki% wat2wasm --debug-names recursive_calls_arg.wat
;; spacetanuki% toywasm --max-frames=5 --wasi recursive_calls_arg.wasm 3
;; spacetanuki% toywasm --max-frames=5 --wasi recursive_calls_arg.wasm 5
;; Error: [trap] stack overflow (5): too many frames
;; frame[  4] funcpc 000108 (<unknown>:recursive_call) callerpc 000118
;;   param [0] = 00000002
;; frame[  3] funcpc 000108 (<unknown>:recursive_call) callerpc 000118
;;   param [0] = 00000003
;; frame[  2] funcpc 000108 (<unknown>:recursive_call) callerpc 000118
;;   param [0] = 00000004
;; frame[  1] funcpc 000108 (<unknown>:recursive_call) callerpc 000128
;;   param [0] = 00000005
;; frame[  0] funcpc 000120 (<unknown>:_start)
;;   local [0] = 00000005
;;    :
;;    :
;; spacetanuki% 

(module
  (func $args_get (import "wasi_snapshot_preview1" "args_get") (param i32 i32) (result i32))
  (func $args_sizes_get (import "wasi_snapshot_preview1" "args_sizes_get") (param i32 i32) (result i32))
  (func $atoi (param $p i32) (result i32) (local $n i32)
    block
      loop
        ;; if (*p == 0) return n;
        local.get $p
        i32.load8_u
        i32.eqz
        if
          local.get $n
          return
        end
        ;; if (*p < '0') break;
        local.get $p
        i32.load8_u
        i32.const 0x30 ;; '0'
        i32.lt_u
        br_if 1
        ;; if (*p > '9') break;
        local.get $p
        i32.load8_u
        i32.const 0x39 ;; '9'
        i32.gt_u
        br_if 1
        ;; n = n * 10 + (*p - '0')
        local.get $p
        i32.load8_u
        i32.const 0x30 ;; '0'
        i32.sub
        local.get $n
        i32.const 10
        i32.mul
        i32.add
        local.set $n
        ;; p++
        local.get $p
        i32.const 1
        i32.add
        local.set $p
        br 0
      end
    end
    unreachable
  )
  (func $get_arg (result i32)
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

    i32.const 0x1000
    i32.load
    i32.const 2
    i32.ne
    if (result i32)
      ;; 1
      i32.const 0x300
    else
      ;; argv[1]
      i32.const 0x1004
      i32.load offset=0x4
    end
  )
  (func $recursive_call (param i32) (result i32)
    local.get 0
    i32.eqz
    if
      local.get 0
      return
    end
    local.get 0
    i32.const 1
    i32.sub
    call $recursive_call
    i32.const 1
    i32.add
  )
  (func $_start (export "_start") (local i32)
    call $get_arg
    call $atoi
    local.tee 0
    call $recursive_call
    local.get 0
    i32.eq
    br_if 0
    unreachable
  )
  (memory (export "memory") 1)

  (data (i32.const 0x300) "1\00")
)
