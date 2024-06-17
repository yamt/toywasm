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
    --threads-enabled=true \
    --tail-call-enabled=true \
    --bulk-memory-enabled=true \
    --exceptions-enabled=true \
    --gc-enabled=false \
    --custom-page-sizes-enabled=true \
    --generate-custom-sections=true \
    --max-memories=10 \
    --max-tables=8 \
    --memory64-enabled=false \
    --multi-value-enabled=true \
    --disallow-traps=false \
    --relaxed-simd-enabled=false \
    --sign-extension-ops-enabled=true \
    -o ${SEED_DIR}/$x
done
