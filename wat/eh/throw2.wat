(module
  (tag $e (param f64 i64))
  (func $g
    f64.const 4321
    i64.const 7890
    throw $e
  )
  (func $f (result f64 i64 exnref)
    try_table (catch_ref $e 0)
      call $g
    end
    unreachable
  )
  (func (export "_start")
    call $f
    drop
    drop
    drop
  )
)
