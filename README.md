## What's this?

A WebAssembly interpreter.

## On-browser demo

You can try it out on [webassembly.sh](https://webassembly.sh/?run-command=toywasm%20--version):

```shell
$ curl -o cowsay.wasm https://registry-cdn.wapm.io/contents/liftm/cowsay/0.2.2/target/wasm32-wasi/release/cowsay.wasm
$ toywasm --wasi cowsay.wasm hello
 _______
< hello >
 -------
        \   ^__^
         \  (oo)\_______
            (__)\       )\/\
               ||----w |
                ||     ||
$
```

Note: [the binary published to wapm.io](https://wapm.io/yamt/toywasm)
is built with an ancient wasi-sdk to workaround
[an webassembly.sh issue](https://github.com/wasmerio/webassembly.sh/issues/105).

## Goals

* Learn the spec by implementing it
* Have a fun
* Correctness
* Clean code
* Small footprint

## Non-goals

* Performance
* Stable API/ABI

## Where can this run?

### The most tested

* macOS/amd64
* wasm32-wasi (on toywasm itself)

### Tested on CI

#### Natively

* macOS/amd64
* Ubuntu/amd64

#### With qemu (less coverage because of slowness)

* Ubuntu/arm64
* Ubuntu/armhf (Note: 32-bit)
* Ubuntu/s390x (Note: big endian)

#### with even less coverage

* wasm32-wasi

### Occasionally tested manually

* wasm32-wasi-pthread
* NuttX/xtensa
* NuttX/sim on macOS/amd64
* NetBSD/amd64

## Which specs are implemented?

* [WebAssembly](https://github.com/WebAssembly/spec) except SIMD.

* [threads](https://github.com/WebAssembly/threads)

* [wasi preview1](https://github.com/WebAssembly/wasi)

* [wasi-threads](https://github.com/WebAssembly/wasi-threads)

## How slow is this?

[benchmark with ffmpeg](./benchmark/ffmpeg.sh)
```
===== ./b/toywasm --wasi --wasi-dir .video --
       89.30 real        89.05 user         0.12 sys
            85729280  maximum resident set size
       1004073618737  instructions retired
            65818624  peak memory footprint
===== wasm3 --dir .video --
       18.71 real        18.51 user         0.09 sys
            85213184  maximum resident set size
        119289794408  instructions retired
            84590592  peak memory footprint
===== iwasm.fast --dir=.video
       22.54 real        22.40 user         0.08 sys
           142921728  maximum resident set size
        144876916114  instructions retired
           142270464  peak memory footprint
===== iwasm.classic --dir=.video
      207.97 real       207.55 user         0.23 sys
            80572416  maximum resident set size
        957347283724  instructions retired
            79970304  peak memory footprint
===== iwasm.fast-jit --dir=.video --jit-codecache-size=100000000 --stack-size=100000
        5.99 real         5.85 user         0.11 sys
           148045824  maximum resident set size
         55799745721  instructions retired
           138383360  peak memory footprint
===== wasmer run --dir .video --
        2.40 real         1.68 user         0.20 sys
           407715840  maximum resident set size
         18364251289  instructions retired
           385863680  peak memory footprint
===== wasmtime run --dir .video --
        1.87 real         1.70 user         0.11 sys
           259403776  maximum resident set size
         19213022301  instructions retired
           168157184  peak memory footprint
```

## Why is this slow?

* Unlike many of interpreters, toywasm aims to execute wasm bytecode
  directly where possible. That is, it doesn't "compile" wasm bytecode
  into intermediate code.

  Unfortunately, wasm is simply not efficient to execute that way.
  It's basically designed to be somehow "compiled" at the load time.

  Many of interpreters out there translate wasm bytecode to their internal
  bytecode for performance reasons. Wasm3 and WAMR "fast" interpreter
  work exactly that way. Even WAMR "classic" interpreter replaces
  some critical instructions in-place.

  While toywasm maps wasm modules read-only and never modifies it in-place,
  it still generates a few types of offline annotations on the bytecode
  to avoid being too slow. While they are smaller than a full translation,
  you might consider them a kind of translation:

  * Jump table.

    This is to speed up branching. Wasm branch instruction don't have
    a cheap way to know the destination address to jump to. Without this
    table, whenever we execute branch instructions, we need to parse
    every instructions the branch would skip over.

    This is optional and can be disabled by the `--disable-jump-table`
    runtime option.

  * Local offset tables.

    This is to speed up access to locals (E.g. `local.get`) in case
    toywasm is built with variable-sized values, which is the default.
    (`-D TOYWASM_USE_SMALL_CELLS=ON`)
    Without this table, an access to a local is O(x) where x is
    the number of locals in the function, including function arguments.

    You can disable them by `--disable-localtype-cellidx`
    and the `--disable-resulttype-cellidx` runtime options.

    When toywasm is built with fixed-sized values,
    (`-D TOYWASM_USE_SMALL_CELLS=OFF`) an access to a local is O(1).
    In that case, this table is not necessary or used, regardless of
    the above mentioned cli options.
    An implementation with fixed-sized values is probably more
    cpu-efficient especially on a 64-bit host. It's probably more
    memory-efficient as well because it doesn't involve the static
    overhead. (this table) The situation might change when we implement
    larger values. (`v128` used by SIMD.)

  * Type annotations for value-polymorphic instructions.

    Some wasm instructions like `drop` works on a value of any types and
    there is no cheap way to know the type at runtime.
    While validating the bytecode, toywasm annotates these instructions
    with the sizes of the values so that the necessary infomation is
    available when executing it later. While it's theoretically possible
    to calculate them at the execution time, it would be something like
    repeating the validation step.

    This is unconditionally enabled if and only if toywasm is built with
    variable-sized values, which is the default.
    (`-D TOYWASM_USE_SMALL_CELLS=ON`)

* I don't like to use huge-switch statements or "labels as values",
  which are well-known techniques to implement efficient interpreters.
