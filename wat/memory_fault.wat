;; % toywasm memory_fault.wasm
;; Error: [trap] out of bounds memory access (3): invalid memory access at 0000 00000000 + 00000000, size 4, meminst size 0
;; 2024-01-17 02:48:08 (1705427288.053832000): [11738e600] instance_execute_func failed with -1
;; 2024-01-17 02:48:08 (1705427288.054427000): [11738e600] invoke failed with -1
;; % iwasm.classic memory_fault.wasm
;; Exception: out of bounds memory access
;; % iwasm.llvm-jit memory_fault.wasm
;; %

(module
  (func (export "_start")
    ;; this load should trap.
    ;; but on some optimizing runtimes it doesn't because of
    ;; dead-code elimination.
    i32.const 0
    i32.load
    drop
  )
  (memory (export "memory") 0)
)
