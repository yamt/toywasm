(module
  (tag $e (param i32))
  (func $g (param i32)
    local.get 0
    throw $e
  )
  (func $rethrow (param exnref)
    local.get 0
    throw_ref
  )
  (func $f (param i32) (result i32)
    block (result exnref)
      try_table (catch_all_ref 0)
        local.get 0
        call $g
      end
      unreachable
    end
    block (param exnref) (result i32)
      try_table (param exnref) (catch $e 0)
        call $rethrow
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
