#! /bin/sh

set -e

TEST_DIR=${TEST_DIR:-.wasm-spec-test}
fetch_spec_json()
{
    REPO=https://github.com/yamt/wasm-spec-test
    REF=fc9a60041c6b3f41d1e11b7714693507f9a52545
    mkdir "${TEST_DIR}"
    git -C "${TEST_DIR}" init
    git -C "${TEST_DIR}" fetch --depth 1 ${REPO} ${REF}
    git -C "${TEST_DIR}" checkout FETCH_HEAD
}
test -d "${TEST_DIR}" || fetch_spec_json
