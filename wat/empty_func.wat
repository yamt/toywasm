
(module
  ;; functions which can be optimized out
  (func $i
    i32.const 0
    if
      call $i
    end
  )
  (func $j
    (local i32)
    local.get 0
    if
      call $j
    end
  )
  (func $h)
  (func $g
    call $h
  )
  (func $f
    call $g
    call $i
    call $j
  )
  (func (export "_start")
    call $f
  )
  (memory (export "memory") 0)
)
