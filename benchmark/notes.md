## misc preparations

* stop large apps (firefox, docker, ...)

* disable automatic backup

  ```
  sudo tmutil disable
  ```

* stop spotlight indexer

  ```
  sudo mdutil -a -i off
  ```

* show the current indexing status

  ```
  mdutil -a -s
  ```

* `sync;sync;sync`

## build instructions

### toywasm

```
cmake -B b -G Ninja .
cmake --build b

cmake -B b.fix -G Ninja -DTOYWASM_USE_SMALL_CELLS=OFF .
cmake --build b.fix

cmake -B b.fix.nosimd -G Ninja -DTOYWASM_USE_SMALL_CELLS=OFF -DTOYWASM_ENABLE_WASM_SIMD=OFF .
cmake --build b.fix.nosimd
```

### wasm3

```
mkdir build
cd build
cmake ..
make
cp wasm3 ~/bin
```

### wasm-micro-runtime

```
git checkout WAMR-2.4.1
cd product-mini/platforms/darwin
```

and then run an equivalent of [build-wamr.sh](build-wamr.sh).

### wazero

installed with homebrew.

### wasmi

```
git checkout v0.51.0
cargo build --package wasmi_cli --profile bench
cp target/release/wasmi_cli ~/bin
```

### wasmedge

```
brew uninstall spdlog
git checkout 0.13.5
mkdir build
cd build
cmake \
-DCMAKE_BUILD_TYPE=Release \
-DWASMEDGE_BUILD_AOT_RUNTIME=OFF \
-DWASMEDGE_BUILD_SHARED_LIB=OFF \
-DWASMEDGE_BUILD_STATIC_LIB=ON \
-DWASMEDGE_LINK_TOOLS_STATIC=ON \
-DWASMEDGE_BUILD_PLUGINS=OFF \
..
make
cp ./tools/wasmedge/wasmedge ~/bin
```

### wasmtime

installed with homebrew.

### wasmer

installed with homebrew.
