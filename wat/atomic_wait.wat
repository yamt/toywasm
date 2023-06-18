(module
  (func (export "_start")
    i32.const 99
    i32.const 0
    i32.const 0
    i64.const 5_000_000_000 ;; 5 sec
    memory.atomic.wait32
    ;; assert a timeout
    i32.const 2
    i32.ne
    if
      unreachable
    end
    drop
  )
  (memory (export "memory") 1 1 shared)
)
