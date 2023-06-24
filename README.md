## What's this?

A WebAssembly interpreter.

## On-browser demo

You can try it out on [webassembly.sh](https://webassembly.sh/?run-command=toywasm):

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
* Portability
  * Core wasm: C11
  * WASI: POSIX-like
  * Clang/LLVM extentions are ok where an alternative implmentation
    in pure C is reasonably easy.

## Non-goals

* Performance
* Stable API/ABI

## What are implemented?

|Feature                 |Status                                            |
| ---------------------- | ------------------------------------------------ |
|[WebAssembly]           |Complete `2.0 (Draft 2022-06-27)`                 |
|[extended-const]        |Complete (`TOYWASM_ENABLE_WASM_EXTENDED_CONST`)   |
|[multi-memory]          |Complete (`TOYWASM_ENABLE_WASM_MULTI_MEMORY`)     |
|[tail-call]             |Complete (`TOYWASM_ENABLE_WASM_TAILCALL`)         |
|[threads]               |Complete (`TOYWASM_ENABLE_WASM_THREADS`)          |
|[wasi_snapshot_preview1]|See [the top comment in wasi.c]                   |
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
`bulk-memory-operations`,
`simd`.

[WebAssembly]: https://github.com/WebAssembly/spec
[extended-const]: https://github.com/WebAssembly/extended-const
[multi-memory]: https://github.com/WebAssembly/multi-memory
[tail-call]: https://github.com/WebAssembly/tail-call
[threads]: https://github.com/WebAssembly/threads
[wasi_snapshot_preview1]: https://github.com/WebAssembly/wasi
[wasi-threads]: https://github.com/WebAssembly/wasi-threads
[the top comment in wasi.c]: libwasi/wasi.c

## Where can this run?

|Platform                |Tested on CI     |Notes                        |
| ---------------------- | --------------- | --------------------------- |
|macOS/amd64             |Yes              |                             |
|wasm32-wasi             |Yes (on toywasm) |                             |
|Ubuntu/amd64            |Yes              |                             |
|Ubuntu/i386             |Yes              |32-bit, smaller alignment    |
|Ubuntu/arm64            |Yes (on qemu)    |                             |
|Ubuntu/armhf            |Yes (on qemu)    |32-bit                       |
|Ubuntu/s390x            |Yes (on qemu)    |Big endian                   |
|Ubuntu/riscv64          |Yes (on qemu)    |                             |
|wasm32-wasi-threads     |No               |Occasionally tested manually |
|NuttX/esp32             |No               |Occasionally tested manually |
|NuttX/sim on macOS/amd64|No               |Occasionally tested manually |
|NetBSD/amd64            |No               |Occasionally tested manually |

## Use as a command

See [toywasm command help message](https://webassembly.sh/?run-command=toywasm).

## Use as a library

See example apps:

* [Investigate module](./examples/app/app.c)

* [Load and execute WASI module](./examples/runwasi/runwasi.c)

## How slow/fast is this?

* [Computation benchmark](./benchmark/ffmpeg.md)

* [Startup benchmark](./benchmark/startup.md)

## Why is this slow?

* Unlike many of interpreters, toywasm aims to execute wasm bytecode
  directly where possible. That is, it doesn't "compile" wasm bytecode
  into intermediate code.

  Unfortunately, wasm bytecode is simply not efficient to execute
  that way. It's basically designed to be somehow "compiled" at the
  load time.

  Many of interpreters out there translate wasm bytecode to their internal
  bytecode for performance reasons. Wasm3 and WAMR "fast" interpreter
  work exactly that way. Even WAMR "classic" interpreter replaces
  some critical instructions in-place.

  While toywasm maps wasm modules read-only and never modifies them in-place,
  it still generates a few types of offline annotations on the bytecode
  to avoid being too slow. While they are smaller than a full translation,
  you might consider them a kind of translation:

  * Jump table.

    This is to speed up branching. Wasm branch instruction don't have
    a cheap way to know the destination address to jump to. Without this
    table, whenever we execute a forward-branching instruction, we need
    to parse every instructions the branch would skip over.

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

    When toywasm is built to use fixed-sized values,
    (`-D TOYWASM_USE_SMALL_CELLS=OFF`) an access to a local is naturally
    O(1).  In that case, this table is not necessary or used, regardless
    of the above mentioned cli options.
    The fixed-size here is 128-bit for the `v128` type if toywasm is
    built with SIMD enabled. (`-D TOYWASM_ENABLE_WASM_SIMD=ON`, which is
    the default) Othewise, it's 64-bit.
    While fixed-sized values incurs up to 4x memory overhead (eg. by using
    128 bits to store an i32 value) for wasm operand stack, wasm locals,
    and wasm tables, it's probably more cpu-efficient especially on a
    64-bit host. Depending on situations, it can be memory-efficient as
    well because it doesn't involve the static overhead of this table.

  * Type annotations for value-polymorphic instructions.

    Some wasm instructions like `drop` works on a value of any types and
    there is no cheap way to know the type at runtime.
    While validating the bytecode, toywasm annotates these instructions
    with the sizes of the values so that the necessary infomation is
    available when executing the code later. While it's theoretically
    possible to calculate them at the execution time, it would be
    something like repeating the validation step. An alternative is
    to annotate each values on the stack. But it's likely even more
    expensive.

    This annotation is unconditionally enabled if and only if toywasm is
    built with variable-sized values, which is the default.
    (`-D TOYWASM_USE_SMALL_CELLS=ON`)

* I don't like to use huge-switch statements or
  [labels as values](https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html),
  which are well-known techniques to implement efficient interpreters.

### Overhead of the annotations

The memory consumption for the above mentioned annotations
depends on the wasm bytecode to annotate.
The following is a few examples taken with wasm modules I happened
to have.

#### toywasm

```
 wasm instructions to annotate       368444 bytes
           jump table overhead        80192 bytes
      type annotation overhead        16568 bytes
  local type cell idx overhead        16480 bytes
 result type cell idx overhead         1006 bytes
```

#### spidermonkey

```
 wasm instructions to annotate      4143921 bytes
           jump table overhead      1166528 bytes
      type annotation overhead       142304 bytes
  local type cell idx overhead       109880 bytes
 result type cell idx overhead         2448 bytes
```

#### ffmpeg

```
 wasm instructions to annotate     15442793 bytes
           jump table overhead      2715248 bytes
      type annotation overhead       335344 bytes
  local type cell idx overhead       388730 bytes
 result type cell idx overhead         5882 bytes
```

## Internals

* [check_interrupt and restartable errors](./doc/check_interrupt.md)
