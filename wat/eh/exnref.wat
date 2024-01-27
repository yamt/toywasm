(module
  (tag $e0)
  (tag $e1 (param i32))
  (tag $e2 (param i32 i64))
  (tag $e3 (param v128))
  (func $wrap-e1 (param i32) (result exnref)
    try_table (catch_all_ref 0)
      local.get 0
      throw $e1
    end
    unreachable
  )
  (func $unwrap-e1 (param exnref) (result i32)
    try_table (catch $e1 0)
      local.get 0
      throw_ref
    end
    unreachable
  )
  (func $f (param i32) (result i32)
    local.get 0
    call $wrap-e1
    call $unwrap-e1
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
