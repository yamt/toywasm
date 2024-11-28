## What's this?

A WebAssembly interpreter written in C.

## Goals

* Learn the spec by implementing it
* Have a fun
* Correctness
* Clean code
* Small footprint
* Reasonable performance
* Portability
  * Core wasm: C11
  * WASI: POSIX-like
  * Clang/LLVM extentions are ok where an alternative implmentation
    in pure C is reasonably easy.

## Non-goals

* Top-notch performance
* Stable API/ABI

## What are implemented?

|Feature                 | cmake config                           | Notes    |
| ---------------------- | -------------------------------------- | -------- |
|[WebAssembly]           |                                        |`2.0 (Draft 2022-06-27)`|
|[extended-const]        |`TOYWASM_ENABLE_WASM_EXTENDED_CONST`    |          |
|[exception-handling]    |`TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING`|See [the top comment in insn_impl_eh.h]|
|[multi-memory]          |`TOYWASM_ENABLE_WASM_MULTI_MEMORY`      |          |
|[tail-call]             |`TOYWASM_ENABLE_WASM_TAILCALL`          |          |
|[threads]               |`TOYWASM_ENABLE_WASM_THREADS`           |          |
|[custom-page-sizes]     |`TOYWASM_ENABLE_WASM_CUSTOM_PAGE_SIZES` |          |
|[wasi_snapshot_preview1]|`TOYWASM_ENABLE_WASI`                   |See [the top comment in wasi.c]|
|[wasi-threads]          |`TOYWASM_ENABLE_WASI_THREADS`           |          |
|[dynamic-linking]       |`TOYWASM_ENABLE_DYLD`                   |          |
|[littlefs for WASI]     |`TOYWASM_ENABLE_WASI_LITTLEFS`          |          |

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
[exception-handling]: https://github.com/WebAssembly/exception-handling
[multi-memory]: https://github.com/WebAssembly/multi-memory
[tail-call]: https://github.com/WebAssembly/tail-call
[threads]: https://github.com/WebAssembly/threads
[custom-page-sizes]: https://github.com/WebAssembly/custom-page-sizes
[wasi_snapshot_preview1]: https://github.com/WebAssembly/wasi
[wasi-threads]: https://github.com/WebAssembly/wasi-threads
[dynamic-linking]: https://github.com/WebAssembly/tool-conventions/blob/main/DynamicLinking.md
[littlefs for WASI]: libwasi_littlefs/README.md
[the top comment in insn_impl_eh.h]: lib/insn_impl_eh.h
[the top comment in wasi.c]: libwasi/wasi.c

## Where can this run?

|Platform                |Tested on CI     |Notes                        |
| ---------------------- | --------------- | --------------------------- |
|macOS/amd64             |Yes              |                             |
|wasm32-wasi             |Yes (on toywasm) |                             |
|Ubuntu/amd64            |Yes              |                             |
|Ubuntu/i386             |Yes              |32-bit, smaller alignment    |
|Ubuntu/arm64            |Yes (on qemu)    |                             |
|Ubuntu/armhf            |[Disabled]       |32-bit                       |
|Ubuntu/s390x            |[Disabled]       |Big endian                   |
|Ubuntu/riscv64          |Yes (on qemu)    |                             |
|Windows/X64             |Yes              |Core wasm only               |
|wasm32-wasi-threads     |No               |Occasionally tested manually |
|NuttX/esp32             |No               |Occasionally tested manually |
|NuttX/sim on macOS/amd64|No               |Occasionally tested manually |
|NetBSD/amd64            |No               |Occasionally tested manually |

[Disabled]: https://github.com/yamt/toywasm/pull/238#issuecomment-2226868990

## Use as a command

See toywasm command help message.

```shell
Usage:
	toywasm [OPTIONS] [--] <MODULE> [WASI-ARGS...]
Options:
	--allow-unresolved-functions
	--disable-jump-table
	--disable-localtype-cellidx
	--disable-resulttype-cellidx
	--dyld
	--dyld-bindnow
	--dyld-dlfcn
	--dyld-path LIBRARY_DIR
	--dyld-stack-size C_STACK_SIZE_FOR_PIE_IN_BYTES
	--invoke FUNCTION[ FUNCTION_ARGS...]
	--load MODULE_PATH
	--max-frames NUMBER_OF_FRAMES
	--max-memory MEMORY_LIMIT_IN_BYTES
	--max-stack-cells NUMBER_OF_CELLS
	--repl
	--repl-prompt STRING
	--print-build-options
	--print-stats
	--timeout TIMEOUT_MS
	--version
	--wasi
	--wasi-dir HOST_DIR[::GUEST_DIR]
	--wasi-env NAME=VAR
	--wasi-littlefs-dir LITTLEFS_IMAGE_PATH::LFS_DIR[::GUEST_DIR]
	--wasi-littlefs-block-size BLOCK_SIZE
	--wasi-littlefs-disk-version DISK_VERSION
Examples:
	Run a wasi module
		toywasm --wasi module
	Load a module and invoke its function
		toywasm --load module --invoke "func arg1 arg2"
```

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

* Release binaries are built with clang, with `ThinLTO` where it's
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

  ```shell
  % cmake -B build
  % cmake --build build
  ```

  For build-time options, see the [ToywasmConfig.cmake].

  By default, it uses ThinLTO if available.
  You might want to use regular LTO instead to get a bit better optimization:

  ```shell
  % cmake -B build . -DUSE_IPO=OFF -DCMAKE_C_FLAGS=-flto=full
  % cmake --build build
  ```

  By default, it requires `wabt` and `cmocka` for tests. If you don't
  want to install them, you can disable tests.

  ```shell
  % cmake -B build -D BUILD_TESTING=OFF
  % cmake --build build
  ```

  Otherwise, after building it, you can run tests with:

  ```shell
  % cd build
  % ctest
  ```

* For `wasm32-wasi` and `wasm32-wasi-threads` targets, we provide
  a convenient script [build-wasm32-wasi.sh] to download wasi-sdk and
  build toywasm with it.

* For NuttX, it's probably easiest to use [interpreters/toywasm in NuttX apps].
  There are a few sample defconfig with toywasm enabled, including
  [sim:toywasm] and [esp32s3-devkit:toywasm].

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

[esp32s3-devkit:toywasm]: https://nuttx.apache.org/docs/latest/platforms/xtensa/esp32s3/boards/esp32s3-devkit/index.html#toywasm
