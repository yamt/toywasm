(module
  (tag $e (param i32))
  (func $dec-and-throw (param i32)
    local.get 0
    i32.eqz
    br_if 0
    local.get 0
    i32.const 1
    i32.sub
    throw $e
  )
  (func $loop (export "loop") (param i32)
    local.get 0
    loop (param i32)
      try_table (param i32) (catch $e 0)
        call $dec-and-throw
      end
    end
  )
  (func $_start (export "_start")
    i32.const 1000000
    call $loop
  )
)
