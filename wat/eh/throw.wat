(module
  (tag $e (param i32))
  (func $g (param i32)
    local.get 0
    throw $e
  )
  (func $f (param i32) (result i32)
    block (result i32)
      try_table (catch $e 0)
        local.get 0
        call $g
      end
      unreachable
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
)
