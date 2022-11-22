#! /bin/sh

set -e

DIR=.wasi-testsuite

fetch() {
    REPO=https://github.com/WebAssembly/wasi-testsuite
    # prod/testsuite-base branch
    REF=24ddb68f31210c761572133a66d29ebb5a0e666c
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
