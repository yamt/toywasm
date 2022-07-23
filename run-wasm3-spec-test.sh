#! /bin/sh

set -e

WASM3_DIR=.wasm3

fetch_wasm3()
{
    REPO=https://github.com/wasm3/wasm3
    REF=9dcfce271c2fac86823725fc9ec0f75309d820e4
    mkdir "${WASM3_DIR}"
    git -C "${WASM3_DIR}" init
    git -C "${WASM3_DIR}" fetch --depth 1 ${REPO} ${REF}
    git -C "${WASM3_DIR}" checkout FETCH_HEAD
}

test -d "${WASM3_DIR}" || fetch_wasm3
cd "${WASM3_DIR}/test"
exec python3 ./run-spec-test.py "$@"
