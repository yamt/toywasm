(module
  (func (export "i16x8.bitmask") (param v128) (result i32)
    local.get 0
    i16x8.bitmask
  )
)

(assert_return
  (invoke "i16x8.bitmask"
    (v128.const i16x8 0xff00 0x00ff 0 0x8000 0x0001 0 0 0)
  )
  (i32.const 9)
)
