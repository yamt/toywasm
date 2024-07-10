;; Note: this module would consume about 8*4GB memory with
;; toywasm on 64-bit arch.

(module
  (func (export "_start")
    table.size 0
    i32.const -2
    i32.ne
    if
      unreachable
    end

    ref.null func
    i32.const 1
    table.grow $t
    i32.const -2
    i32.ne
    if
      unreachable
    end

    table.size 0
    i32.const -1
    i32.ne
    if
      unreachable
    end

    ;; the table is already max possible size, 2^32-1.
    ;; a grow attempt should fail.
    ref.null func
    i32.const 1
    table.grow $t
    i32.const -1
    i32.ne
    if
      unreachable
    end

    table.size 0
    i32.const -1
    i32.ne
    if
      unreachable
    end
  )
  (table $t 0xfffffffe funcref)
)
