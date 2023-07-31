# What's this?

A runtime linker for [WebAssembly Dynamic Linking].

* Compatible with WASI

* Optional [dlopen-like API](../examples/libdl)

# Portability notes

While this library is implemented using the toywasm API directly.
it should be straightforward to port it to other runtime API like
[wasm-c-api] except a few things listed below.

## dylink.0 custom section

The [wasm-c-api] doesn't provide a way to query the contents of custom
sections. You need something runtime-specific.

## PLT trampoline

Maybe simply creating host functions (with `wasm_func_new_with_env`)
and making it call the corresponding wasm function (with `wasm_func_call`)
is enough for most cases. The approach is not compatible with tail call
though.

## WASI and other host functions, including our dlopen-like API

The import/export API of [wasm-c-api] is a bit low-level and cumbersome
to use.

Also, the API to link host functions like WASI varies among
runtimes.
Eg. [wasmer-c-api-wasi], [wasmtime-c-api-wasi]

[WebAssembly Dynamic Linking]: https://github.com/WebAssembly/tool-conventions/blob/main/DynamicLinking.md

[wasm-c-api]: https://github.com/WebAssembly/wasm-c-api

[wasmer-c-api-wasi]: https://docs.rs/wasmer-c-api/4.1.0/wasmer/wasm_c_api/wasi/index.html

[wasmtime-c-api-wasi]: https://docs.wasmtime.dev/examples-c-wasi.html
