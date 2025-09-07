#! /bin/sh

set -e

DIR=.wasi-testsuite

fetch() {
    REPO=https://github.com/WebAssembly/wasi-testsuite
    # prod/testsuite-all branch
    REF=6128c39e78f9282a3323a282f26e764a5fb789fa
    mkdir "${DIR}"
    git -C "${DIR}" init
    git -C "${DIR}" fetch --depth 1 ${REPO} ${REF}
    git -C "${DIR}" checkout FETCH_HEAD
}

if [ ! -d ${DIR} ]; then
    fetch
fi

TOYWASM=${TOYWASM:-toywasm}
TOYWASM_WASI_FILESYSTEM_TYPE=${TOYWASM_WASI_FILESYSTEM_TYPE:-host}
export TOYWASM_WASI_FILESYSTEM_TYPE

FILTER_OPTIONS="--exclude-filter test/wasi-testsuite-skip.json"
if ${TOYWASM} --version | grep -F "sizeof(void *) = 4"; then
    FILTER_OPTIONS="${FILTER_OPTIONS} test/wasi-testsuite-skip-32bit.json"
fi
OS_SKIPFILE="test/wasi-testsuite-skip-$(uname -s).json"
if [ -f ${OS_SKIPFILE} ]; then
    FILTER_OPTIONS="${FILTER_OPTIONS} ${OS_SKIPFILE}"
fi
if [ ${TOYWASM_WASI_FILESYSTEM_TYPE} = littlefs ]; then
    FILTER_OPTIONS="${FILTER_OPTIONS} test/wasi-testsuite-skip-littlefs.json"
fi

TESTS="${TESTS} assemblyscript/testsuite/"
TESTS="${TESTS} c/testsuite/"
TESTS="${TESTS} rust/testsuite/"

for t in ${TESTS}; do
    TESTDIRS="${TESTDIRS} ${DIR}/tests/${t}"
done

# Note: test directories left from the previous runs can confuse
# the test runner. https://github.com/WebAssembly/wasi-testsuite/issues/81
# until it gets fixed in the upstream, use find, which doesn't follow
# symlinks.
find ${DIR}/tests -name "*.cleanup" | xargs rm -rf

virtualenv venv
. ./venv/bin/activate
if [ ${TOYWASM_WASI_FILESYSTEM_TYPE} = littlefs ]; then
    python3 -m pip install littlefs-python
fi
python3 -m pip install -r ${DIR}/test-runner/requirements.txt
python3 test/pipe.py |
python3 ${DIR}/test-runner/wasi_test_runner.py \
-t ${TESTDIRS} \
${FILTER_OPTIONS} \
-r test/wasi-testsuite-adapter.py
