## What's this?

A WebAssembly interpreter written in C.

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
|[dynamic-linking]       |Complete (`TOYWASM_ENABLE_DYLD`)                  |

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
[dynamic-linking]: https://github.com/WebAssembly/tool-conventions/blob/main/DynamicLinking.md
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
|Ubuntu/riscv64          |Yes (on qemu)    |[Temporarily disabled]       |
|wasm32-wasi-threads     |No               |Occasionally tested manually |
|NuttX/esp32             |No               |Occasionally tested manually |
|NuttX/sim on macOS/amd64|No               |Occasionally tested manually |
|NetBSD/amd64            |No               |Occasionally tested manually |

[Temporarily disabled]: https://github.com/yamt/toywasm/issues/80

## Use as a command

See [toywasm command help message](https://webassembly.sh/?run-command=toywasm).

## Use as a library

See example apps:

* [Investigate module](./examples/app/app.c)

* [Load and execute WASI module](./examples/runwasi/runwasi.c)

Toywasm provides cmake config files for its libraries.
If your app is using cmake, you can use `find_package` to find toywasm
libraries as it's done in the [CMakeLists.txt](./examples/runwasi/CMakeLists.txt)
of the above example apps.

## Release binaries

> **Warning**
> Toywasm version numbers are NOT a semver.
> It's just an increasing number, which doesn't imply anything
> about compatibilities or features.

* Release binaries are built with clang, with `thin LTO` where it's
  available.

* For macOS, we ship a universal binary. (amd64 and arm64)

* We ship binaries with two configurations for each platforms/architectures.
  The assets with `full-` in the filename have most features enabled.
  Another one is with the default configuration.

* Each asset contains the `toywasm` cli command, static libraries,
  and cmake config files.

## Build from source

* To build toywasm, we recommend to use clang for the following features,
  which some parts of toywasm codebase is kind of assuming to have.
  They are not required for the correctness though.

  * [`musttail` attribute] (LLVM 13 and later)

  * [LTO]

* For macOS and similar posix-like environment, you can build it with
  cmake in a usual way.

  ```
  % cmake -B build
  % cmake --build build
  ```

  For build-time options, see the [ToywasmConfig.cmake].

  By default, it requires `wabt` and `cmocka` for tests. If you don't
  want to install them, you can disable tests.

  ```
  % cmake -B build -D BUILD_TESTING=OFF
  % cmake --build build
  ```

  Otherwise, after building it, you can run tests with:

  ```
  % cd build
  % ctest
  ```

* For `wasm32-wasi` and `wasm32-wasi-threads` targets, we provide
  a convenient script [build-wasm32-wasi.sh] to download wasi-sdk and
  build toywasm with it.

* For NuttX, it's probably easiest to use [interpreters/toywasm in NuttX apps].
  There are a few sample defconfig with toywasm enabled, including [sim:toywasm].

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
  bytecode for performance reasons. [Wasm3] and [WAMR "fast" interpreter]
  work exactly that way. Even WAMR "classic" interpreter replaces
  some critical instructions in-place.

  While toywasm maps wasm modules read-only and never modifies them in-place,
  it still generates a few types of offline annotations on the bytecode
  to avoid being too slow. While they are smaller than a full translation,
  you might consider them a kind of translation. See [Annotations] for details.

* I don't like to use huge-switch statements or
  [labels as values GNU C extension], which are well-known techniques to
  implement efficient interpreters.

## Internals

* [Annotations]

* [check_interrupt and restartable errors](./doc/check_interrupt.md)

## References

* [Wasm3]

* [WAMR "fast" interpreter]

* [labels as values GNU C extension]

[Wasm3]: https://github.com/wasm3/wasm3/blob/main/docs/Interpreter.md

[WAMR "fast" interpreter]: https://www.intel.com/content/www/us/en/developer/articles/technical/webassembly-interpreter-design-wasm-micro-runtime.html

[labels as values GNU C extension]: https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html

[Annotations]: ./doc/annotations.md

[build-wasm32-wasi.sh]: ./build-wasm32-wasi.sh

[ToywasmConfig.cmake]: ./cmake/ToywasmConfig.cmake

[`musttail` attribute]: https://clang.llvm.org/docs/AttributeReference.html#musttail

[LTO]: https://www.llvm.org/docs/LinkTimeOptimization.html

[interpreters/toywasm in NuttX apps]: https://github.com/apache/nuttx-apps/blob/master/interpreters/toywasm/Kconfig

[sim:toywasm]: https://nuttx.apache.org/docs/latest/platforms/sim/sim/boards/sim/index.html#toywasm
