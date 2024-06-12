;; return the n-th number in fibonacci sequence

(module
  (func $fib-slow (export "fib-slow") (param i64) (result i64)
    local.get 0
    i64.const 2
    i64.le_u
    if
      i64.const 1
      return
    end
    local.get 0
    i64.const 1
    i64.sub
    call $fib-slow
    local.get 0
    i64.const 2
    i64.sub
    call $fib-slow
    i64.add
  )

  (func $fib-recurse-sub (param i64 i64 i64) (result i64)
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
    call $fib-recurse-sub
  )

  (func $fib-recurse (export "fib-recurse") (param i64) (result i64)
    local.get 0
    i64.const 0
    i64.const 1
    call $fib-recurse-sub
  )

  (func $fib-loop (export "fib-loop") (param i64) (result i64)
    (local i64 i64)
    i64.const 1
    local.set 1
    loop
      local.get 0
      i64.eqz
      if
        local.get 2
        return
      end
      local.get 0
      i64.const 1
      i64.sub
      local.set 0
      local.get 1
      local.get 2
      i64.add
      local.get 2
      local.set 1
      local.set 2
      br 0
    end
    unreachable
  )

  (func $sqrt-5 (result f64)
    f64.const 5
    f64.sqrt
  )

  (func $phi (result f64)
    call $sqrt-5
    f64.const 1
    f64.add
    f64.const 2
    f64.div
  )

  (func $pow (param f64 i64) (result f64)
    f64.const 1
    loop (param f64)
      local.get 1
      i64.eqz
      if (param f64) (result f64)
        return
      end
      local.get 1
      i64.const 1
      i64.sub
      local.set 1
      local.get 0
      f64.mul
      br 0
    end
    unreachable
  )

  (func $fib-math (export "fib-math") (param i64) (result i64)
    call $phi ;; f64.const 1.618033988749895
    local.get 0
    call $pow
    call $sqrt-5 ;; f64.const 2.23606797749979
    f64.div
    f64.const 0.5
    f64.add
    f64.floor
    i64.trunc_f64_u
  )

  (func $test-one (param i32 i64 i64)
    local.get 1
    local.get 0
    call_indirect (param i64) (result i64)
    local.get 2
    i64.ne
    if
      unreachable
    end
  )

  (func $test-func (param i32)
    (call $test-one (local.get 0) (i64.const 1) (i64.const 1))
    (call $test-one (local.get 0) (i64.const 2) (i64.const 1))
    (call $test-one (local.get 0) (i64.const 3) (i64.const 2))
    (call $test-one (local.get 0) (i64.const 4) (i64.const 3))
    (call $test-one (local.get 0) (i64.const 5) (i64.const 5))
    (call $test-one (local.get 0) (i64.const 12) (i64.const 144))
    (call $test-one (local.get 0) (i64.const 25) (i64.const 75025))
    (call $test-one (local.get 0) (i64.const 50) (i64.const 12586269025))
    (call $test-one (local.get 0) (i64.const 70) (i64.const 190392490709135))
    ;; note: fib-math doesn't match after f(70).
  )

  (func (export "test-all")
    i32.const 0
    call $test-func
    i32.const 1
    call $test-func
    i32.const 2
    call $test-func
    ;; commented out because fib-slow is very slow
    ;; i32.const 3
    ;; call $test-func
  )

  (table 4 funcref)
  (elem (i32.const 0) $fib-recurse $fib-loop $fib-math $fib-slow)
)
