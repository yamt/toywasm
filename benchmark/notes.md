## misc preparations

* stop large apps (firefox, docker, ...)

* stop backup

  ```
  sudo tmutil disable
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
git checkout WAMR-1.2.2
cd product-mini/platforms/darwin
```

and then run an equivalent of [build-wamr.sh](build-wamr.sh).

### wazero

installed with homebrew.

### wasmi

```
git checkout v0.30.0
carge build --release
cp target/release/wasmi_cli ~/bin
```

### wasmedge

```
git checkout 0.12.1
mkdir build
cd build
cmake .. -DWASMEDGE_BUILD_AOT_RUNTIME=OFF -DWASMEDGE_BUILD_SHARED_LIB=OFF
make
cp ./tools/wasmedge/wasmedge ~/bin
```

### wasmtime

installed with homebrew.

### wasmer

installed with homebrew.
