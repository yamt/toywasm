#! /bin/sh

set -e

WASM_TOOLS=${WASM_TOOLS:-wasm-tools}
SEED_DIR=seed
mkdir -p ${SEED_DIR}
for x in $(seq 10000); do
    dd if=/dev/urandom bs=10240 count=100 2> /dev/null | ${WASM_TOOLS} smith \
    --max-imports=8 \
    --allow-invalid-funcs=true \
    --reference-types-enabled=true \
    --simd-enabled=true \
    -o ${SEED_DIR}/$x
done
