# What's this

An example of a host function for Binaryen's log-execution pass.

# Example

```
% wasm-opt -g -o toywasm.log.wasm --log-execution=env toywasm.wasm
% log-execution toywasm.log.wasm
log-execution idx=00000004 callerpc=00145e (toywasm:_start)
log-execution idx=00000000 callerpc=00144d (toywasm:__wasm_call_ctors)
log-execution idx=00000198 callerpc=007416 (toywasm:__main_void)
log-execution idx=00000205 callerpc=007531 (toywasm:__wasi_args_sizes_get)
log-execution idx=00000073 callerpc=003795 (toywasm:malloc)
log-execution idx=00000089 callerpc=0037b8 (toywasm:dlmalloc)
log-execution idx=00000074 callerpc=003896 (toywasm:dlmalloc)
log-execution idx=00000074 callerpc=003896 (toywasm:dlmalloc)
log-execution idx=00000074 callerpc=003896 (toywasm:dlmalloc)
log-execution idx=00000074 callerpc=003896 (toywasm:dlmalloc)
log-execution idx=00000074 callerpc=003896 (toywasm:dlmalloc)
log-execution idx=00000074 callerpc=003896 (toywasm:dlmalloc)
log-execution idx=00000074 callerpc=003896 (toywasm:dlmalloc)
log-execution idx=00000074 callerpc=003896 (toywasm:dlmalloc)
log-execution idx=00000088 callerpc=004ba2 (toywasm:dlmalloc)
log-execution idx=00000106 callerpc=00569c (toywasm:calloc)
log-execution idx=00000089 callerpc=0037b8 (toywasm:dlmalloc)
log-execution idx=00000088 callerpc=004ba2 (toywasm:dlmalloc)
log-execution idx=00000678 callerpc=011989 (toywasm:memset)
log-execution idx=00000677 callerpc=011b18 (toywasm:memset)
log-execution idx=00000105 callerpc=0056fd (toywasm:calloc)
log-execution idx=00000204 callerpc=00751e (toywasm:__wasi_args_get)
log-execution idx=00000007 callerpc=0014af (toywasm:main)
   :
   : (omit)
   :
```
