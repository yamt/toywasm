#! /bin/sh

set -e

TEST_DIR=${TEST_DIR:-.wasm-spec-test}
fetch_spec_json()
{
    REPO=https://github.com/yamt/wasm-spec-test
    REF=cf0d19c6d687b702667c105c6036cd4d8aa18678
    mkdir "${TEST_DIR}"
    git -C "${TEST_DIR}" init
    git -C "${TEST_DIR}" fetch --depth 1 ${REPO} ${REF}
    git -C "${TEST_DIR}" checkout FETCH_HEAD
}
test -d "${TEST_DIR}" || fetch_spec_json
