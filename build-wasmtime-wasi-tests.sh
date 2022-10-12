#! /bin/sh

set -e

DIR=.wasmtime
fetch_wasmtime()
{
    REPO=https://github.com/bytecodealliance/wasmtime
    REF=81bff71d11103db08e112865f9fdd80c2d7593f9
    mkdir -p "${DIR}"
    git -C "${DIR}" init
    git -C "${DIR}" fetch --depth 1 ${REPO} ${REF}
    git -C "${DIR}" checkout FETCH_HEAD
}

test -d "${DIR}" || fetch_wasmtime
(cd "${DIR}/crates/test-programs/wasi-tests" && \
cargo build --target wasm32-wasi)

WASM_DIR=.wasmtime-wasi-tests-bin
mkdir "${WASM_DIR}"
cp \
"${DIR}"/crates/test-programs/wasi-tests/target/wasm32-wasi/debug/*.wasm \
"${WASM_DIR}"
