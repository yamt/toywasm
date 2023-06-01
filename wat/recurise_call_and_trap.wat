;; https://github.com/bytecodealliance/wasm-micro-runtime/pull/2233

(module
  (func $f (param i32)
    block
      local.get 0
      i32.load8_u
      i32.eqz
      br_if 0
      unreachable
    end
    local.get 0
    i32.const 1
    i32.add
    call $f
    loop
      br 0
    end
  )
  (func (export "_start")
    i32.const 0
    call $f
  )
  (memory (export "memory") 1)
  (data (i32.const 100) "\01")
)
