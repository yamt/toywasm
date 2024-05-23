(module
  (func (export "i32-ok")
    i32.const 27
    i32.load
    drop
  )
  (func $i32-trap (export "i32-trap")
    i32.const 28
    i32.load
    drop
  )
  (func (export "i8-ok")
    i32.const 30
    i32.load8_u
    drop
  )
  (func (export "i8-trap")
    i32.const 31
    i32.load8_u
    drop
  )
  (func (export "i32-ok-after-grow")
    i32.const 1
    memory.grow
    i32.const -1
    i32.eq
    if
      unreachable
    end
    call $i32-trap
  )
  (memory 31 (pagesize 1))
)
