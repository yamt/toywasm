(module
  (func $fib (export "fib") (param i64 i64 i64) (result i64)
    local.get 0
    i64.eqz
    if
      local.get 1
      return
    end
    local.get 0
    i64.const 1
    i64.sub
    local.get 2
    local.get 1
    local.get 2
    i64.add
    call $fib
  )

  ;; return the (n+1) th number in fibonacci sequence
  (func (export "fibn") (param i64) (result i64)
    local.get 0
    i64.const 0
    i64.const 1
    call $fib
  )
)
