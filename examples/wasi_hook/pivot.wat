(module
  (type $fd_read_type (func (param i32 i32 i32 i32) (result i32)))
  (func (export "fd_read") (type $fd_read_type)
    local.get 0
    local.get 1
    local.get 2
    local.get 3
    i32.const 0
    call_indirect $imports (type $fd_read_type)
  )
  (table $imports (export "$imports") 1 1 funcref)
)
