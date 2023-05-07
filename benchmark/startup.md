# Startup benchmark

## What's this

[startup.sh](./startup.sh) is a benchmark intended to measure
start-up time and memory footprint.
For each runtimes, it instantiates a large module (`ffmpeg.wasm`,
which is about 20MB) and executes only a small part of it. (just
print the `-version` message)

## Result

![Result](./startup.png)

* Run on a macOS/amd64 laptop.

* [Raw values](./startup.txt)

* Plotted with [plot.py](./plot.py).

## Observations

* The default wasm3 performs best.
  However, it's mainly because of its lazy compilation/validation.
  With lazy compilation/validation disabled with the `--compile` option,
  it has certain compilation/validation overhead as shown in the
  `wasm3 (no lazy)` row.

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
  Unlike wasm3, it doesn't defer the validation though. Disabling
  the lazy compilation (`-DWAMR_BUILD_LAZY_JIT=0`) doesn't make a much
  difference as I expected. I'm not sure why.

* Toywasm's annotations have small but measurable overheads.
  cf. [Overhead of the annotations (ffmpeg)](../README.md#ffmpeg)

* It's common for JIT-based runtimes to spawn many compilation threads
  to improve startup time. (thus "user" far larger than "real")

* Some of runtimes involve surprisingly large RSS like 600MB.
  I'm not sure why.
