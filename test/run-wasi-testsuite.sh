#! /bin/sh

set -e

DIR=.wasi-testsuite

fetch() {
    REPO=https://github.com/WebAssembly/wasi-testsuite
    # prod/testsuite-base branch
    REF=1bd1bdb1613f1ac5d1c30d49ae98db3fd19c24d6
    mkdir "${DIR}"
    git -C "${DIR}" init
    git -C "${DIR}" fetch --depth 1 ${REPO} ${REF}
    git -C "${DIR}" checkout FETCH_HEAD
}

if [ ! -d ${DIR} ]; then
    fetch
fi

virtualenv venv
. ./venv/bin/activate
python3 -m pip install -r ${DIR}/test-runner/requirements.txt
python3 ${DIR}/test-runner/wasi_test_runner.py \
-t ${DIR}/tests/assemblyscript/testsuite/ \
${DIR}/tests/c/testsuite/ \
-r test/wasi-testsuite-adapter.py
