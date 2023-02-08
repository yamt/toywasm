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

## Which specs are implemented?

* [WebAssembly](https://github.com/WebAssembly/spec) except SIMD.

* [extended-const](https://github.com/WebAssembly/extended-const)

* [multi-memory](https://github.com/WebAssembly/multi-memory)

* [tail-call](https://github.com/WebAssembly/tail-call)

* [threads](https://github.com/WebAssembly/threads)

* [wasi_snapshot_preview1](https://github.com/WebAssembly/wasi)

* [wasi-threads](https://github.com/WebAssembly/wasi-threads)

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

* wasm32-wasi-threads
* NuttX/xtensa
* NuttX/sim on macOS/amd64
* NetBSD/amd64

## How slow is this?

* reasonable performance for an interpreter
* far slower than jit-based engines (as expected)

[benchmark with ffmpeg](./benchmark/ffmpeg.sh)
```
+++++++++++ Interpreters +++++++++++
===== toywasm v0.0.12-47-g5195d36
----- ./b/toywasm --wasi --wasi-dir .video --
       80.81 real        80.76 user         0.04 sys
            85704704  maximum resident set size
        928886373006  instructions retired
            65818624  peak memory footprint
===== Wasm3 v0.5.0 on x86_64
----- wasm3 --dir .video --
       17.76 real        17.72 user         0.03 sys
            84811776  maximum resident set size
        119202854389  instructions retired
            84189184  peak memory footprint
===== iwasm 1.1.2 (fast interpreter)
----- iwasm.fast --dir=.video
       21.51 real        21.45 user         0.05 sys
           142340096  maximum resident set size
        145416287370  instructions retired
           141680640  peak memory footprint
===== iwasm 1.1.2 (classic interpreter)
----- iwasm.classic --dir=.video
      204.87 real       204.75 user         0.07 sys
            80601088  maximum resident set size
        972808351888  instructions retired
            79994880  peak memory footprint
===== wasmedge version 0.12.0-alpha.1-18-g656ffd1c (interpreter)
----- wasmedge --dir .video --
      191.43 real       191.16 user         0.23 sys
           656846848  maximum resident set size
       1849697059414  instructions retired
           629571584  peak memory footprint
+++++++++++ JIT ++++++++++++++++++++
===== iwasm 1.1.2 (fast jit)
----- iwasm.fast-jit --dir=.video --jit-codecache-size=100000000
        3.47 real         6.12 user         0.15 sys
           150413312  maximum resident set size
         55907165813  instructions retired
           139563008  peak memory footprint
===== wasmer 2.3.0
----- wasmer run --dir .video --
        7.46 real        53.53 user         2.91 sys
           782438400  maximum resident set size
        215363514668  instructions retired
           573227008  peak memory footprint
===== wasmtime-cli 3.0.1
----- wasmtime run --dir .video --
        6.86 real        43.54 user         1.64 sys
           442699776  maximum resident set size
        179148836698  instructions retired
           368766976  peak memory footprint
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

* I don't like to use huge-switch statements or
  [labels as values](https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html),
  which are well-known techniques to implement efficient interpreters.
