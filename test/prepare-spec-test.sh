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
    while read WAST; do
        D=${TEST_DIR}/$(dirname ${WAST})
        mkdir -p ${D}
        ${WAST2JSON} --enable-all -o ${TEST_DIR}/${WAST%%.wast}.json ${SPEC_DIR}/${WAST}
    done
}

${WAST2JSON} --version

fetch_spec .spec https://github.com/WebAssembly/spec opam-2.0.0
(cd ${SPEC_DIR} && find test -name "*.wast") | compile .

fetch_spec .spec-threads https://github.com/WebAssembly/threads 8e1a7de753fbe6455c33e670352bdfe43b8cc5bd
# Note: we don't have the test harness necessary for threads.wast
(cd .spec-threads && find test -name "atomic.wast") | compile threads

fetch_spec .spec-tail-call https://github.com/WebAssembly/tail-call 8d7be0b84f992d6350f1df3d9b9d4159d5083b0f
(cd .spec-tail-call && find test -name "return_call*.wast") | compile tail-call

# currently memory-multi.wast is handled manually because it has simd tests
fetch_spec .spec-multi-memory https://github.com/WebAssembly/multi-memory 07fc57c48fbc87b098fae622cd10278bb95ef576
(cd .spec-multi-memory && find test \
-name "load.wast" -o \
-name "store.wast" -o \
-name "memory_size.wast" -o \
-name "memory_grow.wast") | compile multi-memory

fetch_spec .spec-extended-const https://github.com/WebAssembly/extended-const dd72ab9676e27d4c3fbf48030115e4ee64e05507
(cd .spec-extended-const && find test \
-name "data.wast" -o \
-name "global.wast") | compile extended-const
