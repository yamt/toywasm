(module
  (tag $e)
  (func $throw-null
    try_table (catch $e 0)
      ref.null exn
      throw_ref
    end
  )
  (func (export "_start")
    call $throw-null
  )
)
