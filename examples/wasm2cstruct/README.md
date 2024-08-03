# What's this

This program loads and validates a module and generates a C source code
which provides a structure which represents the loaded module.
(`struct module`)

```shell
% wasm2cstruct g_wasm_module foo.wasm | clang-format > foo.c
```

Note: the first argument is the C symbol to use.

The generated C source file contains a single exported symbol for the
`struct module`.

You can compile and link it to you program, which can look like:

```c
extern struct module g_wasm_module;

/* instantiate the preloaded module */
instance_create(..., &g_wasm_module, ...);
```

* This effectively preloads a module at the build-time of the embedder.

* The generated structure and its all dependencies are `const` qualified.
  The compiler likely places them into a read-only sections.

* While it's less flexible than dynamically loading modules using
  the `module_create` API, it's likely more memory-efficient.
  More importantly, the loaded modules can be placed on the ROM directly.

* You can't use `module_destroy` on the generated structure.
