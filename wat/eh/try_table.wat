(module
  (tag $e (param i32))
  (func $f (param i32) (result i32)
    block (result i32)
      try_table (result i32) (catch $e 0)
        local.get 0
      end
    end
  )
  (func (export "_start")
    i32.const 1234
    call $f
    i32.const 1234
    i32.ne
    if
      unreachable
    end
  )
  (memory (export "memory") 0)
)
