# What's this

A simple wasi vfs implementation backed by [littlefs].

[littlefs]: https://github.com/littlefs-project/littlefs

# Prerequisites

Checkout littlefs source code in as "littlefs" in this directory.

```shell
git clone https://github.com/littlefs-project/littlefs
```

# Build

Build toywasm with the following options.

```
TOYWASM_ENABLE_WASI=ON
TOYWASM_ENABLE_WASI_LITTLEFS=ON
```

# Usage

You can use the `--wasi-mapdir-littlefs` cli option to add preopens
backed by littlefs-backed.

Or, make your embedder use the `wasi_instance_prestat_add_mapdir_littlefs`
api.

# Example

Copy a file from a littlefs image to host /tmp.

```shell
toywasm --wasi --wasi-mapdir /tmp::host --wasi-mapdir-littlefs littlefs.bin::/::lfs uutils.async.wasm cp lfs/pi2.js host/
```

# Caveats

Curretly, block size and other filesystem parameters are hardcoded.
