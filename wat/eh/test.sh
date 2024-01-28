#! /bin/sh

set -e
set -x
TOYWASM=${TOYWASM:-${TEST_RUNTIME_EXE:-toywasm}}
for wat in *.wat; do
    wasm=${wat%%.wat}.wasm
    ${TOYWASM} ${wasm}
done
