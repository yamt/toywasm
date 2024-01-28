#! /bin/sh

set -e
set -x
TOYWASM=${TOYWASM:-toywasm}
for wat in *.wat; do
    wasm=${wat%%.wat}.wasm
    ${TOYWASM} ${wasm}
done
