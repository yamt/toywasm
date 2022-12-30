#! /bin/sh

set -e

THIS_DIR=$(cd $(dirname $0) && pwd -P)

EXE=${1:-${THIS_DIR}/build/toywasm --wasi}

WASM_DIR="${THIS_DIR}/.wasmtime-wasi-tests-bin"
fetch_test_bin()
{
    URL=https://github.com/yamt/wasmtime/releases/download/wasi-tests-bin-20221012/wasmtime-wasi-tests-bin.tgz
    mkdir -p "${WASM_DIR}"
    curl -L ${URL} | (cd "${WASM_DIR}" && pax -rz)
}

test -d "${WASM_DIR}" || fetch_test_bin

SKIPLIST=
BLACKLIST="${THIS_DIR}/wasmtime-wasi-tests-blacklist.txt"

OS_BLACKLIST="${THIS_DIR}/wasmtime-wasi-tests-blacklist-$(uname -s).txt"
if [ -f ${OS_BLACKLIST} ]; then
    BLACKLIST="${BLACKLIST} ${OS_BLACKLIST}"
fi

if [ "${WASM_ON_WASM:-0}" -ne 0 ]; then
    run_wasi_test()
    {
        ../build.native/toywasm ${EXTRA_OPTIONS} --wasi \
        --wasi-dir . --wasi-dir ../build.wasm --wasi-dir ${TMP} -- \
        ../build.wasm/toywasm ${EXTRA_OPTIONS} --wasi \
        --wasi-dir . --wasi-dir ${TMP} -- \
        ${w} ${TMP}
    }
else
    run_wasi_test()
    {
        ${EXE} --wasi-dir ${TMP} ${w} ${TMP};
    }
    if ${EXE} --version | grep -F "sizeof(void *) = 4"; then
        SKIPLIST="${SKIPLIST} ${THIS_DIR}/wasmtime-wasi-tests-skip-32bit.txt"
    fi
fi

# suppress useless messages on macOS
# toywasm(46834,0x11068c600) malloc: nano zone abandoned due to inability to preallocate reserved vm space.
export MallocNanoZone=0

TOTAL=0
FAIL=0
EXPECTED_FAIL=0
UNEXPECTED_SUCCESS=0
SKIPPED=0
cd "${WASM_DIR}"
for w in *.wasm; do
    TOTAL=$((TOTAL + 1))
    if [ -n "${SKIPLIST}" ]; then
        if grep "^${w%%.wasm}$" ${SKIPLIST}; then
            echo "=== ${w} skipped"
            SKIPPED=$((SKIPPED + 1))
            continue
        fi
    fi
    echo "=== ${w}"
    TMP=$(mktemp -d)
    if run_wasi_test; then
        if grep "^${w%%.wasm}$" ${BLACKLIST}; then
            echo "=== ${w} succeeded (unexpected)"
            UNEXPECTED_SUCCESS=$((UNEXPECTED_SUCCESS + 1))
        else
            echo "=== ${w} succeeded (expected)"
        fi
    else
        if grep "^${w%%.wasm}$" ${BLACKLIST}; then
            echo "=== ${w} failed (expected)"
            EXPECTED_FAIL=$((EXPECTED_FAIL + 1))
        else
            echo "=== ${w} failed (unexpected)"
            FAIL=$((FAIL + 1))
        fi
    fi
    rm -r ${TMP}
done
echo "fail / expected_fail / unexpected_success / skipped / total = ${FAIL} / ${EXPECTED_FAIL} / ${UNEXPECTED_SUCCESS} / ${SKIPPED} / ${TOTAL}"
test ${UNEXPECTED_SUCCESS} -eq 0
test ${FAIL} -eq 0
