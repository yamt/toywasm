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

## What are implemented?

|Feature                 |Status                                            |
| ---------------------- | ------------------------------------------------ |
|[WebAssembly]           |Complete except `simd`                            |
|[extended-const]        |Complete (`TOYWASM_ENABLE_WASM_EXTENDED_CONST`)   |
|[multi-memory]          |Complete (`TOYWASM_ENABLE_WASM_MULTI_MEMORY`)     |
|[tail-call]             |Complete (`TOYWASM_ENABLE_WASM_TAILCALL`)         |
|[threads]               |Complete (`TOYWASM_ENABLE_WASM_THREADS`)          |
|[wasi_snapshot_preview1]|See [the top comment in lib/wasi.c]               |
|[wasi-threads]          |Complete (`TOYWASM_ENABLE_WASI_THREADS`)          |

Note: As this runtime is relatively new, all proposals which had finished
when I started this implementation are just included in the `WebAssembly`
in the above table.
It includes
`mutable-global`,
`nontrapping-float-to-int-conversions`,
`sign-extension-ops`,
`multi-value`,
`reference-types`,
`bulk-memory-operations`.

[WebAssembly]: https://github.com/WebAssembly/spec
[extended-const]: https://github.com/WebAssembly/extended-const
[multi-memory]: https://github.com/WebAssembly/multi-memory
[tail-call]: https://github.com/WebAssembly/tail-call
[threads]: https://github.com/WebAssembly/threads
[wasi_snapshot_preview1]: https://github.com/WebAssembly/wasi
[wasi-threads]: https://github.com/WebAssembly/wasi-threads
[the top comment in lib/wasi.c]: lib/wasi.c

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
===== toywasm v0.0.13-79-g271efea
----- ./b/toywasm --wasi --wasi-dir .video --
       84.32 real        84.15 user         0.10 sys
            84635648  maximum resident set size
        929396192777  instructions retired
            64737280  peak memory footprint
===== toywasm v0.0.13-79-g271efea (-DTOYWASM_USE_SMALL_CELLS=OFF)
----- ./b.fix/toywasm --wasi --wasi-dir .video --
       52.11 real        51.99 user         0.07 sys
            85184512  maximum resident set size
        478840775961  instructions retired
            65318912  peak memory footprint
===== toywasm v0.0.13-79-g271efea (annotations disabled)
----- ./b/toywasm --wasi --wasi-dir .video --disable-jump-table --disable-localtype-cellidx --disable-resulttype-cellidx --
      651.73 real       650.58 user         0.75 sys
            82554880  maximum resident set size
       4899305108703  instructions retired
            62656512  peak memory footprint
===== Wasm3 v0.5.0 on x86_64
----- wasm3 --dir .video --
       18.56 real        18.50 user         0.05 sys
            84738048  maximum resident set size
        119248980331  instructions retired
            84115456  peak memory footprint
===== iwasm 1.1.2 (fast interpreter)
----- iwasm.fast --dir=.video
       22.34 real        22.25 user         0.06 sys
           142491648  maximum resident set size
        144888572204  instructions retired
           141811712  peak memory footprint
===== iwasm 1.1.2 (classic interpreter)
----- iwasm.classic --dir=.video
      212.58 real       212.18 user         0.29 sys
            80674816  maximum resident set size
        957643141822  instructions retired
            80023552  peak memory footprint
===== wasmedge version 0.12.0-alpha.1-18-g656ffd1c (interpreter)
----- wasmedge --dir .video --
      198.92 real       198.35 user         0.42 sys
           657440768  maximum resident set size
       1850102075429  instructions retired
           630206464  peak memory footprint
+++++++++++ JIT ++++++++++++++++++++
===== wazero v1.0.0-pre.8
----- wazero run -mount .video --
       15.53 real        20.41 user         0.55 sys
          1632882688  maximum resident set size
        164784908017  instructions retired
          1629159424  peak memory footprint
===== iwasm 1.1.2 (fast jit)
----- iwasm.fast-jit --dir=.video --jit-codecache-size=100000000
        3.60 real         6.30 user         0.16 sys
           149508096  maximum resident set size
         55909532233  instructions retired
           138776576  peak memory footprint
===== wasmer 2.3.0
----- wasmer run --dir .video --
        7.84 real        56.94 user         2.66 sys
           780304384  maximum resident set size
        215264459564  instructions retired
           577720320  peak memory footprint
===== wasmtime-cli 3.0.1
----- wasmtime run --dir .video --
        7.06 real        46.80 user         1.57 sys
           445968384  maximum resident set size
        179141534421  instructions retired
           365096960  peak memory footprint
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
