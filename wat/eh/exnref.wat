(module
  (tag $e0)
  (tag $e1 (param i32))
  (tag $e2 (param i32 i64))
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
  (func $wrap-e2 (param i32 i64) (result exnref)
    try_table (catch_all_ref 0)
      local.get 0
      local.get 1
      throw $e2
    end
    unreachable
  )
  (func $unwrap-e2 (param exnref) (result i32 i64)
    try_table (catch $e2 0)
      local.get 0
      throw_ref
    end
    unreachable
  )
  (func $f (param i32 i64) (result i32 i64)
    local.get 0
    call $wrap-e1
    call $unwrap-e1
    local.get 1
    call $wrap-e2
    call $unwrap-e2
  )
  (func (export "_start")
    i32.const 1234
    i64.const 0x4444_3333_2222_1111
    call $f
    i64.const 0x4444_3333_2222_1111
    i64.ne
    if
      unreachable
    end
    i32.const 1234
    i32.ne
    if
      unreachable
    end
  )
)
