#! /bin/sh

set -e

WAST2JSON=${WAST2JSON:-wast2json}

fetch_spec()
{
    SPEC_DIR=$1
    REPO=$2
    REF=$3
    if test -d ${SPEC_DIR}; then
        return
    fi
    mkdir "${SPEC_DIR}"
    git -C "${SPEC_DIR}" init
    git -C "${SPEC_DIR}" fetch --depth 1 ${REPO} ${REF}
    git -C "${SPEC_DIR}" checkout FETCH_HEAD
}

compile()
{
    TEST_DIR=$1
    shift 1
    while read WAST; do
        D=${TEST_DIR}/$(dirname ${WAST})
        mkdir -p ${D}
        ${WAST2JSON} --enable-all "$@" -o ${TEST_DIR}/${WAST%%.wast}.json ${SPEC_DIR}/${WAST}
    done
}

${WAST2JSON} --version

# Note: --no-check for https://github.com/WebAssembly/wabt/issues/2201
fetch_spec .spec https://github.com/WebAssembly/spec f3a0e06235d2d84bb0f3b5014da4370613886965
(cd ${SPEC_DIR} && find test -name "*.wast") | compile . --no-check

fetch_spec .spec-threads https://github.com/WebAssembly/threads 09f2831349bf409187abb6f7868482a8079f2264
# Note: we don't have the test harness necessary for threads.wast
(cd .spec-threads && find test -name "atomic.wast") | compile threads

fetch_spec .spec-tail-call https://github.com/WebAssembly/tail-call 6f44ca27af411a0f6bc4e07520807d7adfc0de88
(cd .spec-tail-call && find test -name "return_call*.wast") | compile tail-call

# https://github.com/WebAssembly/multi-memory/pull/46
fetch_spec .spec-multi-memory https://github.com/WebAssembly/multi-memory 4f6b8f53ec11e59f5e38033db4199db18df83706
(cd .spec-multi-memory && find test \
-name "load.wast" -o \
-name "store.wast" -o \
-name "memory_size.wast" -o \
-name "memory_grow.wast" -o \
-path "*/multi-memory/*.wast" -a ! -name "memory_copy1.wast") \
| compile multi-memory

fetch_spec .spec-extended-const https://github.com/WebAssembly/extended-const dd72ab9676e27d4c3fbf48030115e4ee64e05507
(cd .spec-extended-const && find test \
-name "data.wast" -o \
-name "global.wast") | compile extended-const

# REVISIT: wabt (wast2json) doesn't implement the latest exception-handling
# yet. https://github.com/WebAssembly/wabt/issues/2348
#fetch_spec .spec-exception-handling https://github.com/WebAssembly/exception-handling d12346cf9a10ddeeef0fcf0c08819755e1a4ac4a
#(cd .spec-exception-handling && find test \
#-name "try_catch.wast" -o \
#-name "try_delegate.wast" -o \
#-name "throw.wast" -o \
#-name "rethrow.wast") | compile exception-handling
