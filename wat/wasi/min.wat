;; a minimum wasi command

(module
  (func (export "_start")
    ;; returning from _start is an equivalent of proc_exit(0).
    ;; (at least it's what wasi-libc as of writing this assumes.)
  )
  (memory (export "memory") 0)
)
