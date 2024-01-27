(module
  (tag $e (param i32))
  (func $g (param i32)
    local.get 0
    throw $e
  )
  (func $f (param i32) (result i32)
    block (result)
      try_table (catch_all 0)
        local.get 0
        call $g
      end
      unreachable
    end
    local.get 0
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
