(module
  (func $f
    return_call $g
  )
  (func $g (export "_start")
    return_call $f
  )

  ;; a dummy memory export to appease wamr.
  ;; https://github.com/bytecodealliance/wasm-micro-runtime/issues/2097
  (memory (export "memory") 0)
)
