(module
  (tag $e (param i32 i32 i32))
  (func $return-or-throw (param i32 i32 i32) (result i32 i32)
    local.get 0
    i32.eqz
    if
      local.get 1
      local.get 2
      return
    end
    local.get 0
    i32.const 1
    i32.sub
    local.get 2
    local.get 1
    local.get 2
    i32.add
    throw $e
  )
  (func $fib (export "fib") (param i32) (result i32 i32)
    local.get 0
    i32.const 1
    i32.const 1
    loop (param i32 i32 i32) (result i32 i32)
      try_table (param i32 i32 i32) (result i32 i32) (catch $e 0)
        call $return-or-throw
      end
    end
  )
  (func (export "_start")
    i32.const 30
    call $fib
    i32.const 2178309
    i32.ne
    if
      unreachable
    end
    i32.const 1346269
    i32.ne
    if
      unreachable
    end
    i32.const 1000
    call $fib
    i32.const 2874523960
    i32.ne
    if
      unreachable
    end
    i32.const 1318412525
    i32.ne
    if
      unreachable
    end
    i32.const 100000
    call $fib
    i32.const 1553270488
    i32.ne
    if
      unreachable
    end
    i32.const 679394397
    i32.ne
    if
      unreachable
    end
  )
)
