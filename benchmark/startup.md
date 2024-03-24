# Startup benchmark

## What's this

[startup.sh](./startup.sh) is a benchmark intended to measure
start-up time and memory footprint.
For each runtimes, it instantiates a large module (`ffmpeg.wasm`,
which is about 20MB) and executes only a small part of it. (just
print the `-version` message)

## Result

![Result](./startup.png)

* Run on a macOS/amd64 laptop:

  ```
  MacBook Pro (15-inch, 2018)
  2.2 GHz 6-Core Intel Core i7
  ```

* [Raw values](./startup.txt)

* Plotted with [plot.py](./plot.py).

## Observations

* The default wasm3 and wasmi with lazy option (`wasmi (lazy)` row) perform
  best. However, it's mainly because of their lazy compilation/validation.
  With lazy compilation/validation disabled, they have certain
  compilation/validation overhead as shown in the `wasm3 (no lazy)` and
  `wasmi` rows.

  Note: While lazy validation is
  [explicitly allowed by the spec](https://webassembly.github.io/spec/core/appendix/implementation.html#validation),
  it's a bit [controversial](https://github.com/WebAssembly/design/issues/1464)
  and thus many of runtimes don't implement it.
  Specifically, toywasm intentionally doesn't implement it because it
  complicates shared modules.

* Toywasm and WAMR classic interpreter are second best.
  It's expected as they don't involve complex compilation processes.

* WAMR fast-jit seems lightweight for a JIT-based runtime as it's
  advertized.
  It also uses a lazy compilation strategy by default.
  Unlike wasm3 and wasmi, it doesn't defer the validation though.
  The performance with lazy compilation is a bit unstable, probably
  because of [a naive locking](https://github.com/bytecodealliance/wasm-micro-runtime/issues/2499).

* Toywasm's annotations have small but measurable overheads.
  cf. [Overhead of the annotations (ffmpeg)](../doc/annotations.md#ffmpeg)

* It's common for JIT-based runtimes to spawn many compilation threads
  to improve startup time. (thus "user" far larger than "real")

* Some of runtimes involve surprisingly large RSS like 600MB.
  I'm not sure why.
