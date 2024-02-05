(module
  (func (export "_start")
    block
      br 0
      try_table
        i32.const 0
        if
          try_table
          end
        end
        block
          try_table
          end
        end
        i32.const 0
        if
          try_table
          end
        else
          try_table
          end
        end
        loop
          try_table
          end
        end
        unreachable
      end
    end
  )
)
