#! /bin/sh

set -e

CC=${CC:-/opt/wasi-sdk-20.0/bin/clang}
TOYWASM=${TOYWASM:-toywasm}

wat2wasm pivot.wat

${CC} -o app.wasm app.c
yes 0123456789|${TOYWASM} --wasi app.wasm

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

# note: hook.wasm shouldn't have data/bss as it borrows
# the linear memory from app.wasm.
if wasm2wat hook.wasm | grep '^ *(data '; then
	echo "unexpected data segments in hook.wasm"
	exit 1
fi

${TOYWASM} --trace=5 --wasi --repl < repl.txt
#${TOYWASM} --wasi --repl < repl.txt
