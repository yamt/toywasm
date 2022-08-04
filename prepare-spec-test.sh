#! /bin/sh

set -e

SPEC_DIR=.spec
TEST_DIR=.opam-2.0.0

fetch_spec()
{
    REPO=https://github.com/WebAssembly/spec
    REF=opam-2.0.0
    mkdir "${SPEC_DIR}"
    git -C "${SPEC_DIR}" init
    git -C "${SPEC_DIR}" fetch --depth 1 ${REPO} ${REF}
    git -C "${SPEC_DIR}" checkout FETCH_HEAD
}

wast2json --version
test -d "${SPEC_DIR}" || fetch_spec
(cd ${SPEC_DIR} && find test -name "*.wast") | while read WAST; do
	D=${TEST_DIR}/$(dirname ${WAST})
    mkdir -p ${D}
    wast2json --enable-all -o ${TEST_DIR}/${WAST%%.wast}.json ${SPEC_DIR}/${WAST}
done
