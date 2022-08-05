#! /bin/sh

set -e

WASM3_DIR=.wasm3

fetch_wasm3()
{
    REPO=https://github.com/yamt/wasm3
    REF=bf10fe461e77fa3c532c0c823d632b6e348895f5
    mkdir "${WASM3_DIR}"
    git -C "${WASM3_DIR}" init
    git -C "${WASM3_DIR}" fetch --depth 1 ${REPO} ${REF}
    git -C "${WASM3_DIR}" checkout FETCH_HEAD
}

test -d "${WASM3_DIR}" || fetch_wasm3
cd "${WASM3_DIR}/test"
exec python3 ./run-spec-test.py "$@"
