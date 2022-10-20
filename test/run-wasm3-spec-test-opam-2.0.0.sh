#! /bin/sh

TEST_DIR=.opam-2.0.0
fetch_spec_json()
{
    REPO=https://github.com/yamt/wasm-spec-test
    REF=opam-2.0.0
    mkdir "${TEST_DIR}"
    git -C "${TEST_DIR}" init
    git -C "${TEST_DIR}" fetch --depth 1 ${REPO} ${REF}
    git -C "${TEST_DIR}" checkout FETCH_HEAD
}
test -d "${TEST_DIR}" || fetch_spec_json

exec ./test/run-wasm3-spec-test.sh --spec-dir $(pwd -P)/${TEST_DIR}/test "$@"
