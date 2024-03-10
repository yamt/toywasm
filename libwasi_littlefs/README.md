# What's this

A simple wasi vfs implementation backed by [littlefs].

[littlefs]: https://github.com/littlefs-project/littlefs

# Prerequisites

Checkout littlefs source code in as "littlefs" in this directory.

```shell
git clone https://github.com/littlefs-project/littlefs
```

You can override the directory with the `TOYWASM_LITTLEFS_SOURCE_DIR`
cmake variable.

# Build

Build toywasm with the following options.

```
TOYWASM_ENABLE_WASI=ON
TOYWASM_ENABLE_WASI_LITTLEFS=ON
```

# Usage

You can use the `--wasi-littlefs-dir` cli option to add preopens
backed by a littlefs file image.

Or, make your embedder use the `wasi_instance_prestat_add_littlefs`
api.

# Example

Copy a file from a littlefs image to host /tmp.

```shell
toywasm --wasi --wasi-dir /tmp::host --wasi-littlefs-dir littlefs.bin::/::lfs cp.wasm lfs/srcfile host/dstfile
```

# WASI incompatibility notes

Littlefs have some incompatibilities with POSIX/WASI semantics.

While some of them can be emulated with user-attributes,
we don't bother to do so.

* littlefs doesn't have unique file-id suitable for WASI file "inode number".
* littlefs doesn't have file timestamps.
* littlefs doesn't have pread/pwrite.
* littlefs doesn't have fallocate.
* littlefs doesn't have symlinks.
* littlefs doesn't have hardlinks.
* In littlefs, opened files are snapshot of the files at the time of open.

# Caveats

Curretly, block size (4096) and other filesystem parameters are hardcoded.
