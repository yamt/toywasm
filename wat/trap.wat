;; cli exit code used for a trap: 
;;
;; toywasm   1
;; wasm3     1
;; wamr      1
;; wasmtime  134  128+SIGABRT
;; wasmer    134  i guess it's same as wasmtime
;; wazero    0    bug? https://github.com/tetratelabs/wazero/issues/1360
;; wasmi_cli 1

(module
  (func (export "_start")
    unreachable
  )
  (memory (export "memory") 0)
)
