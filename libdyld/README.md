# What's this?

A runtime linker for [WebAssembly Dynamic Linking].

* Compatible with WASI

* Optional [dlopen-like API](../examples/libdl)

# TODO

* Have a cache of loaded modules (struct module) so that they can be
  actually shared among programs. A simple mechanism for a limited list
  of libraries (eg. libc) might be good enough.

# Portability notes

While this library is implemented using the toywasm API directly,
it should be straightforward to port it to other runtime APIs like
[wasm-c-api] except a few things listed below.

## dylink.0 custom section

The [wasm-c-api] doesn't provide a way to query the contents of custom
sections. You need something runtime-specific.

## PLT trampoline

Maybe simply creating host functions (with `wasm_func_new_with_env`)
and making it call the corresponding wasm function (with `wasm_func_call`)
is enough for most cases. The approach is not safe if the call is made
with [tail call] instructions though. Also, with the [exception-handling]
proposal, such a host function need to propagate execeptions approprately.
As of writing this, [wasm-c-api] doesn't have an API to deal with exceptions.

For toywasm, we use the [restart mechanism] to call the function to maintain
the [tail call] guarantee. As it doesn't leave host frames, it doesn't
interfere exceptions either. See [dyld_plt.c].

## WASI and other host functions, including our dlopen-like API

The import/export API of [wasm-c-api] is a bit low-level and cumbersome
to use.

Also, the API to link host functions like WASI varies among runtimes.
Eg. [wasmer-c-api-wasi], [wasmtime-c-api-wasi]

[WebAssembly Dynamic Linking]: https://github.com/WebAssembly/tool-conventions/blob/main/DynamicLinking.md

[wasm-c-api]: https://github.com/WebAssembly/wasm-c-api

[wasmer-c-api-wasi]: https://docs.rs/wasmer-c-api/4.1.0/wasmer/wasm_c_api/wasi/index.html

[wasmtime-c-api-wasi]: https://docs.wasmtime.dev/examples-c-wasi.html

[restart mechanism]: ../doc/check_interrupt.md

[dyld_plt.c]: dyld_plt.c

[tail call]: https://github.com/WebAssembly/tail-call

[exception-handling]: https://github.com/WebAssembly/exception-handling
