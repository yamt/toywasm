(module
  (func (export "_start")
    i32.const 0
    i32.load offset=0xffff_fffc
    i32.const 0x1234_5678
    i32.ne
    if
      unreachable
    end
  )
  (memory (export "memory") 65536)
  (data (i32.const 0xffff_fffc) "\78\56\34\12")
)
