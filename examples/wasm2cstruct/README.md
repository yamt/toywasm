# What's this

This program loads and validates a module and generates a C source code
which provides a structure which represents the loaded module.
(`struct module`)

```shell
% wasm2cstruct g_wasm_module foo.wasm | clang-format > module.c
```

Note: the first argument is the C symbol to use.

Note: `clang-format` here is just for possible human-readers of the module.

The generated C source file contains a single exported symbol for the
`struct module`.

You can compile and link it to you program, which can look like:

```c
extern struct module g_wasm_module;

/* instantiate the preloaded module */
instance_create(..., &g_wasm_module, ...);
```

See [runwasi_cstruct] for a complete example to consume
the generated C source file.

[runwasi_cstruct]: ../runwasi_cstruct

## Notes

* This effectively preloads a module at the build-time of the embedder.

* The generated structure and its all dependencies are `const` qualified.
  The compiler likely places them into read-only sections.

* While it's less flexible than dynamically loading modules using
  the `module_create` API, it's likely more memory-efficient
  especially when you want to save the malloc'ed memory.
  More importantly, this can allow placing the loaded modules into
  the ROM directly.

* You can't use `module_destroy` on the generated structure.
  Note that `module_destroy` is the only operation in toywasm which
  modifies a loaded module.

* Maybe it makes sense to use this for modules like `libc.so`.

* A compiler for a modern language might be able to do something similar
  via compile-time code execution. Unfortunately C is not such a language.
