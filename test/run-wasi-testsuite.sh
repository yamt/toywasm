#! /bin/sh

set -e

DIR=.wasi-testsuite

fetch() {
    REPO=https://github.com/WebAssembly/wasi-testsuite
    # prod/testsuite-base branch
    REF=c484b7f846aaa4049d42adaea1ae041b307e3471
    mkdir "${DIR}"
    git -C "${DIR}" init
    git -C "${DIR}" fetch --depth 1 ${REPO} ${REF}
    git -C "${DIR}" checkout FETCH_HEAD
}

if [ ! -d ${DIR} ]; then
    fetch
fi

TOYWASM=${TOYWASM:-toywasm}

FILTER_OPTIONS=
if ${TOYWASM} --version | grep -F "sizeof(void *) = 4"; then
    FILTER_OPTIONS="${FILTER_OPTIONS} --exclude-filter test/wasi-testsuite-skip-32bit.json"
fi

virtualenv venv
. ./venv/bin/activate
python3 -m pip install -r ${DIR}/test-runner/requirements.txt
python3 ${DIR}/test-runner/wasi_test_runner.py \
-t ${DIR}/tests/assemblyscript/testsuite/ \
${DIR}/tests/c/testsuite/ \
${FILTER_OPTIONS} \
-r test/wasi-testsuite-adapter.py
