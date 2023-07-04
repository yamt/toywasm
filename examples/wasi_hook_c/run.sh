#! /bin/sh

set -e

CC=${CC:-/opt/wasi-sdk-20.0/bin/clang}
TOYWASM=${TOYWASM:-toywasm}

wat2wasm pivot.wat

${CC} -o app.wasm app.c

CFLAGS="-O3 -mreference-types -Wall -Werror"
${CC} -c ${CFLAGS} hook-asm.S
${CC} -c ${CFLAGS} hook-c.c
${CC} -c ${CFLAGS} format.c
# note: --import-memory makes the linker to use __wasm_init_memory
${CC} -v \
-Wl,--no-entry \
-Wl,--export=__wasm_call_ctors \
-Xlinker --import-memory=app,memory \
-nostdlib \
-o hook.wasm hook-asm.o hook-c.o format.o -lc

${TOYWASM} --trace=5 --wasi --repl < repl.txt
#${TOYWASM} --wasi --repl < repl.txt
