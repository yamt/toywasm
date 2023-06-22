#! /bin/sh

wat2wasm -r -o f.o f.wat
/opt/wasi-sdk-20.0/bin/clang -o test.wasm main.c f.o g.c
