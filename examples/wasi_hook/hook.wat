;; can this be written in C? maybe...
;;
;; * the latest wasm-ld can import memory with arbitrary names.
;;   cf. https://reviews.llvm.org/D135898
;;
;; * is it possible to import table with a certain name?
;;   otherwise, maybe we can make pivot.wat export a function
;;   which takes a funcref.
;;
;; * as SP global is not exported from the main app,
;;   we need to allocate linear memory and set up our own SP.
;;   note: to allocate memory with memory.grow in this module,
;;   you might need to use the latest wasi-libc for the main app.
;;   cf. https://github.com/WebAssembly/wasi-libc/pull/377
;;
;; * probably it's more straightforward to export an explicit
;;   "initialize the table and SP" function instead of using
;;   an active element.
(module
  (type $fd_read_type (func (param i32 i32 i32 i32) (result i32)))
  (memory (import "app" "memory") 1)
  (table (import "pivot" "$imports") 1 1 funcref)
  (func $my_fd_read (type $fd_read_type)
    ;; assume large enough buffer
    local.get 1 ;; iovec
    i32.load ;; base
    i32.const 0x776f654d ;; "Meow"
    i32.store
    i32.const 4 ;; strlen("Meow")
  )
  (elem (i32.const 0) $my_fd_read)
)
