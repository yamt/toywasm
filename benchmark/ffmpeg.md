# A benchmark with ffmpeg

## What's this

[A simple benchmark with ffmpeg](./benchmark/ffmpeg.sh), intended to
measure computation performance with a real application.

For each runtimes, it converts a video frame with using `ffmpeg.wasm` and
measure the time taken.

## Result

```
+++++++++++ Interpreters +++++++++++
===== toywasm v0.0.16-3-ga2d6bc9 (default configuration)
----- ./b/toywasm --wasi --wasi-dir .video --
       86.71 real        86.58 user         0.08 sys
            86257664  maximum resident set size
        929670392407  instructions retired
            66359296  peak memory footprint
===== toywasm v0.0.16-3-ga2d6bc9 (-DTOYWASM_USE_SMALL_CELLS=OFF, likely faster)
----- ./b.fix/toywasm --wasi --wasi-dir .video --
       50.40 real        50.33 user         0.04 sys
            84967424  maximum resident set size
        479797274314  instructions retired
            65101824  peak memory footprint
===== toywasm v0.0.16-3-ga2d6bc9 (annotations disabled, very slow)
----- ./b/toywasm --wasi --wasi-dir .video --disable-jump-table --disable-localtype-cellidx --disable-resulttype-cellidx --
      682.84 real       680.84 user         0.64 sys
            81661952  maximum resident set size
       4898710336993  instructions retired
            61763584  peak memory footprint
===== Wasm3 v0.5.0 on x86_64
----- wasm3 --dir .video --
       17.61 real        17.57 user         0.02 sys
            85032960  maximum resident set size
        119194907960  instructions retired
            84410368  peak memory footprint
===== iwasm 1.1.2 (fast interpreter)
----- iwasm.fast --dir=.video
       21.39 real        21.33 user         0.04 sys
           142237696  maximum resident set size
        144849415636  instructions retired
           141557760  peak memory footprint
===== iwasm 1.1.2 (classic interpreter)
----- iwasm.classic --dir=.video
      202.76 real       202.70 user         0.05 sys
            80588800  maximum resident set size
        957134541184  instructions retired
            79937536  peak memory footprint
===== wasmedge version 0.12.0-alpha.1-18-g656ffd1c (interpreter)
----- wasmedge --dir .video --
      189.95 real       189.61 user         0.23 sys
           656748544  maximum resident set size
       1849723828297  instructions retired
           629514240  peak memory footprint
===== wasmi_cli 0.27.0
----- wasmi_cli --dir .video --
      100.43 real       100.32 user         0.07 sys
           135770112  maximum resident set size
        779646884133  instructions retired
           114585600  peak memory footprint
===== wazero v1.0.0-pre.9 (interpreter)
----- wazero run -interpreter -mount .video --
      156.02 real       159.32 user         0.57 sys
          1653088256  maximum resident set size
       2013758295761  instructions retired
          1645486080  peak memory footprint
+++++++++++ JIT ++++++++++++++++++++
===== iwasm 1.1.2 (fast jit)
----- iwasm.fast-jit --dir=.video --jit-codecache-size=100000000
        3.39 real         5.94 user         0.13 sys
           149753856  maximum resident set size
         55884564376  instructions retired
           139005952  peak memory footprint
===== wasmer 2.3.0
----- wasmer run --dir .video --
        7.05 real        49.11 user         2.37 sys
           791056384  maximum resident set size
        215496279124  instructions retired
           567320576  peak memory footprint
===== wasmtime-cli 3.0.1
----- wasmtime run --dir .video --
        6.49 real        40.89 user         1.42 sys
           437530624  maximum resident set size
        179230894816  instructions retired
           361287680  peak memory footprint
===== wazero v1.0.0-pre.9
----- wazero run -mount .video --
       14.54 real        19.36 user         0.45 sys
          1641086976  maximum resident set size
        164555653646  instructions retired
          1637302272  peak memory footprint
```

* Run on a macOS/amd64 laptop:

  ```
  MacBook Pro (15-inch, 2018)
  2.2 GHz 6-Core Intel Core i7
  ```

## Observations

* toywasm performs reasonably well for an interpreter.
  It's slower than the fastest interpreters though.

* toywasm with annotations disabled is very slow.

* toywasm's memory consumption is reasonable as well.
  (See "maximum resident set size" and "peak memory footprint".
  I don't know what exactly these metrics reported by macOS's time(1)
  mean though. Also, for some reasons, these numbers seem a bit unstable
  and vary among runs. If you know better metrics to show memory footprint,
  preferably available on macOS, please let us know.)

* toywasm is far slower than JIT-based engines as expected.

* Many of JIT-based engines use multiple threads to hide its
  compilation time. (You can see their "user" time is often longer
  than "real" because of this.)
