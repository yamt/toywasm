#! /bin/sh

set -e
set -x
TOYWASM=${TOYWASM:-toywasm}
for wat in *.wat; do
    wasm=${wat##.wat}.wasm
    wasm-tools parse -o ${wasm} ${wat}
    wasm-tools validate -f all ${wasm} 
    ${TOYWASM} ${wasm}
done
