(module
  (type $fd_read_type (func (param i32 i32 i32 i32) (result i32)))
  (func (export "fd_read") (type $fd_read_type)
    local.get 0
    local.get 1
    local.get 2
    local.get 3
    i32.const 0
    call_indirect $func-table (type $fd_read_type)
  )
  (func (export "set_func") (param i32 funcref)
    local.get 0
    local.get 1
    table.set $func-table
  )
  (table $func-table 1 1 funcref)
)
