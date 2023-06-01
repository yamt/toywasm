(module
  (func (export "_start")
    loop
      br 0
    end
  )

  ;; a dummy memory export to appease wamr.
  ;; https://github.com/bytecodealliance/wasm-micro-runtime/issues/2097
  (memory (export "memory") 0)
)
