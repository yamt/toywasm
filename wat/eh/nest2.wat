(module
  (tag $e1 (param i32))
  (tag $e2 (param i32))
  (tag $e3 (param i32))
  (func $throw (param i32 i32)
    local.get 1
    block (param i32) (result i32)
      block (param i32) (result i32)
        block (param i32) (result i32)
          local.get 0
          br_table 0 1 2
        end
        throw $e1
      end
      throw $e2
    end
    throw $e3
  )
  (func $rethrow (param exnref)
    block (result i32)
      block (result i32)
        block (result i32)
          try_table (catch $e1 0) (catch $e2 1) (catch $e3 2)
            local.get 0
            throw_ref
          end
          unreachable
        end
        i32.const 1
        i32.add
        throw $e1
      end
      i32.const 2
      i32.add
      throw $e2
    end
    i32.const 3
    i32.add
    throw $e3
  )
  (func $f (param i32 i32) (result i32)
    block
      try_table (result i32) (catch $e1 1) (catch_all 0)
        try_table (result i32 exnref) (catch $e2 2) (catch_all 1)
          try_table (result i32 exnref) (catch_ref $e1 0) (catch $e3 3) (catch_all 2)
            try_table (result i32 exnref) (catch_ref $e2 0) (catch_all 3)
              try_table (result i32 exnref) (catch_ref $e1 0) (catch_all 4)
                try_table (result i32 exnref) (catch_ref $e2 0) (catch_ref $e3 1) (catch_all 5)
                  try_table (result i32 exnref) (catch_ref $e1 0) (catch_all 6)
                    try_table (result i32 exnref) (catch_ref $e2 0) (catch_all 7)
                      try_table (result i32 exnref) (catch_ref $e1 0) (catch_ref $e3 1) (catch_all 8)
                        try_table (result i32 exnref) (catch_ref $e2 0) (catch_all 9)
                          try_table (result i32 exnref) (catch_ref $e1 0) (catch_all 10)
                            try_table (result i32) (catch_ref $e2 0) (catch_ref $e3 1)
                              i32.const 0
                              block
                                local.get 0
                                local.get 1
                                call $throw
                              end
                            end
                            unreachable
                          end
                          call $rethrow
                          unreachable
                        end
                        call $rethrow
                        unreachable
                      end
                      call $rethrow
                      unreachable
                    end
                    call $rethrow
                    unreachable
                  end
                  call $rethrow
                  unreachable
                end
                call $rethrow
                unreachable
              end
              call $rethrow
              unreachable
            end
            call $rethrow
            unreachable
          end
          call $rethrow
          unreachable
        end
        call $rethrow
        unreachable
      end
      unreachable
    end
    unreachable
  )
  (func (export "_start")
    i32.const 0
    i32.const 0x1000
    call $f
    i32.const 0x1005 ;; 0x1000 + 5 * 1
    i32.ne
    if
      unreachable
    end
    i32.const 1
    i32.const 0x2000
    call $f
    i32.const 0x200a ;; 0x2000 + 5 * 2
    i32.ne
    if
      unreachable
    end
    i32.const 2
    i32.const 0x3000
    call $f
    i32.const 0x3009 ;; 0x3000 + 3 * 3
    i32.ne
    if
      unreachable
    end
  )
)
