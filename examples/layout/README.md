# What's this

This program loads a WASM module and dumps th data layout
in its linear memory.

The following is an example of the output:
```
memory [0] min 00010000 max 100000000
global [  1] 00000000          __tls_base
global [  2] 00000000          GOT.data.internal.__memory_base
global [  3] 00000001          GOT.data.internal.__table_base
data   [  0] 00000400-00000ea0 .rodata
data   [  1] 00000ea0-00000f94 .data
global [  0] 000033e0          __stack_pointer
```
