(module
  (func (export "f") (param v128) (result v128)
    i32.const 0
    local.get 0
    v128.load8_lane 3
  )
  (memory 1)
  (data (i32.const 0) "\ff")
)

(assert_return
  (invoke "f"
    (v128.const i8x16 127 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0)
  )
  (v128.const i16x8 127 0xff00 0 0 0 0 0 1)
)
