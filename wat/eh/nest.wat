(module
  (tag $e1 (param i32))
  (tag $e2)
  (func $h (param i32)
    block
      local.get 0
      throw $e1
    end
  )
  (func $g (param i32)
    local.get 0
    call $h
  )
  (func $f (param i32) (result i32)
    block
      try_table (result i32) (catch_all 0)
        try_table (result i32) (catch $e1 0) (catch_all 1)
          block (result i32)
            try_table (result i32)
            i32.const 0
              block
                local.get 0
                call $g
              end
            end
          end
        end
      end
      return
    end
    unreachable
  )
  (func (export "_start")
    i32.const 1234
    call $f
    i32.const 1234
    i32.ne
    if
      unreachable
    end
  )
)
