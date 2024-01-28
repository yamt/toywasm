#! /bin/sh

set -e
set -x
TOYWASM=${TOYWASM:-${TEST_RUNTIME_EXE:-toywasm}}

for wat in *.wat; do
    wasm=${wat%%.wat}.wasm
    ${TOYWASM} ${wasm}
done

for wat in trap/*.wat; do
    wasm=${wat%%.wat}.wasm
    ${TOYWASM} ${wasm} 2>&1 | grep '\[trap\]'
done
