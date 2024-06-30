(module
  (func $f
    i32.const 1
    return_call_indirect 0
  )
  (func $g (export "_start")
    i32.const 0
    return_call_indirect 0
  )

  ;; a dummy memory export to appease wamr.
  ;; https://github.com/bytecodealliance/wasm-micro-runtime/issues/2097
  (memory (export "memory") 0)
  (table 2 funcref)
  (elem (i32.const 0) $f $g)
)
