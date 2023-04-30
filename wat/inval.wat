;; a test module with an invalid function.
;;
;; lazy validation is a bit controversial topic.
;; references:
;; https://github.com/WebAssembly/design/issues/1464
;; https://webassembly.github.io/spec/core/appendix/implementation.html#validation
;;
;; % wat2wasm --no-check inval.wat
;; % toywasm inval.wasm
;; 2023-04-30 23:26:50 (1682864810.027295000): load/validation error: error (22) while decoding section (code)
;; load/validation error: error (22) while decoding section (code)
;; 2023-04-30 23:26:50 (1682864810.027797000): module_load failed
;; 2023-04-30 23:26:50 (1682864810.027810000): load failed
;; % wasm3 inval.wasm
;; % wasm3 --compile inval.wasm
;; % wasm3 -f inval inval.wasm
;; Error: compiling function underran the stack
;; % iwasm inval.wasm
;; WASM module load failed: type mismatch, opcode drop was found but stack was empty
;; %

(module
  (func (export "inval")
    drop
  )
  (func (export "_start")
  )
  (memory (export "memory") 0)
)
