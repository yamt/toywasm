(module
  (tag $e1 (param i32 i32 i32 i32))
  (func $wrap-e1 (param i32 i32 i32 i32) (result exnref)
    try_table (catch_all_ref 0)
      local.get 0
      local.get 1
      local.get 2
      local.get 3
      throw $e1
    end
    unreachable
  )
  (func $test-nonnull (local $tmp exnref)
    i32.const 0
    i32.const 0
    i32.const 0
    i32.const 0
    call $wrap-e1
    local.tee $tmp
    ref.is_null
    if
      unreachable
    end
  )
  (func $test-null
    ref.null exn
    ref.is_null
    i32.eqz
    if
      unreachable
    end
  )
  (func $test-null-from-local (local exnref)
    local.get 0
    ref.is_null
    i32.eqz
    if
      unreachable
    end
  )
  (func $test-null-from-table
    i32.const 0
    table.get 0
    ref.is_null
    i32.eqz
    if
      unreachable
    end
  )
  (func $test-null-from-global
    global.get $g
    ref.is_null
    i32.eqz
    if
      unreachable
    end
  )
  (func $test-nonnull-from-global
    global.get $g-nonnull
    ref.is_null
    if
      unreachable
    end
  )
  (func $init
    i32.const 0
    i32.const 0
    i32.const 0
    i32.const 0
    call $wrap-e1
    global.set $g-nonnull
  )
  (func (export "_start")
    call $init
    call $test-nonnull
    call $test-null
    call $test-nonnull-from-global
    call $test-null-from-table
    call $test-nonnull
    call $test-null-from-local
    call $test-nonnull
    call $test-null-from-global
    call $test-nonnull-from-global
  )
  (table $table 1 1 exnref)
  (global $g exnref (ref.null exn))
  (global $g-nonnull (mut exnref) (ref.null exn))
)
