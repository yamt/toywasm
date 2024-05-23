(module
  (func (export "_start")
    ;; memory.grow here is expected to fail because it's already max-sized.
    i32.const 1
    memory.grow 0
    i32.const -1
    i32.ne
    if
      unreachable
    end

    memory.size 0
    i32.const -1
    i32.ne
    if
      unreachable
    end
  )
  (memory 0xffffffff (pagesize 1))
)
