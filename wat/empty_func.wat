
(module
  (func $f)
  (func (export "_start")
    call $f
  )
  (memory (export "memory") 0)
)
