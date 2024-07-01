# What's this

A sample program to make a semantic copy of a wasm module.

This program parses and validates a wasm module and then generate
a semantically same wasm module.

```shell
% wasm2wasm in.wasm out.wasm
```

Note: it doesn't necessarily produce the identical file like cp(1).

Note: it doesn't preserve custom sections.

Note: if TOYWASM_SORT_EXPORTS=ON, it doesn't preserve the order of
exports in the module.
