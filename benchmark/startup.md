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

* Toywasm's annotations have small but measurable overheads.
  cf. [Overhead of the annotations (ffmpeg)](../README.md#ffmpeg)

* The default wasm3 performs best. However,
  it's mainly because of its lazy compilation and validation.
  Without it, it has certain compilation overhead as shown in
  `wasm3 (lazy compilation disabled)`.

  Note: While lazy compiltation/validation is explicitly allowed by the spec,
  it's a bit contreversial feature and thus many of runtimes don't implement
  it. Toywasm intentionally doesn't implement it because it complicates
  shared modules.

* WAMR fast-jit also uses a lazy compilation strategy by defaulst.
  Unlike wasm3, it doesn't defer the validation though. Disabling
  the lazy compilation (`WASM_ENABLE_LAZY_JIT=0`) doesn't make much
  difference as I expeceted. I'm not sure why.

* Toywasm and WAMR classic interpreter are second best.
  It's expected as they don't involve complex compilation processes.
