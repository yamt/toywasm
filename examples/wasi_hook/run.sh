#! /bin/sh

set -e

CC=${CC:-/opt/wasi-sdk-19.0/bin/clang}
TOYWASM=${TOYWASM:-toywasm}

wat2wasm pivot.wat
${CC} -o app.wasm app.c
wat2wasm hook.wat
${TOYWASM} --wasi --repl < repl.txt
