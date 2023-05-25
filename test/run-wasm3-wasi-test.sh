#! /bin/sh

set -e

WASM3_DIR=.wasm3

fetch_wasm3()
{
    REPO=https://github.com/yamt/wasm3
    REF=5edcb0bc405be225ec9a469b0c49f4c72bc7856c
    mkdir "${WASM3_DIR}"
    git -C "${WASM3_DIR}" init
    git -C "${WASM3_DIR}" fetch --depth 1 ${REPO} ${REF}
    git -C "${WASM3_DIR}" checkout FETCH_HEAD
}

test -d "${WASM3_DIR}" || fetch_wasm3
cd "${WASM3_DIR}/test"
exec python3 ./run-wasi-test.py "$@"
