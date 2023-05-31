(module
  (func (export "ceil") (param i64) (result f64)
    i32.const 0
    local.get 0
    i64.store
    i32.const 0
    f64.load
    f64.ceil
  )
  (func (export "floor") (param i64) (result f64)
    i32.const 0
    local.get 0
    i64.store
    i32.const 0
    f64.load
    f64.floor
  )
  (func (export "nearest") (param i64) (result f64)
    i32.const 0
    local.get 0
    i64.store
    i32.const 0
    f64.load
    f64.nearest
  )
  (func (export "trunc") (param i64) (result f64)
    i32.const 0
    local.get 0
    i64.store
    i32.const 0
    f64.load
    f64.trunc
  )
  (func (export "sqrt") (param i64) (result f64)
    i32.const 0
    local.get 0
    i64.store
    i32.const 0
    f64.load
    f64.sqrt
  )
  (memory 1)
)

;; ceil(sNaN) -> qNaN
(assert_return (invoke "ceil" (i64.const 0x7ff4000000000000)) (f64.const nan:arithmetic))
(assert_return (invoke "floor" (i64.const 0x7ff4000000000000)) (f64.const nan:arithmetic))
(assert_return (invoke "nearest" (i64.const 0x7ff4000000000000)) (f64.const nan:arithmetic))
(assert_return (invoke "trunc" (i64.const 0x7ff4000000000000)) (f64.const nan:arithmetic))
(assert_return (invoke "sqrt" (i64.const 0x7ff4000000000000)) (f64.const nan:arithmetic))
