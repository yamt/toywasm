#! /bin/sh

set -e

TEST_DIR=${TEST_DIR:-.wasm-spec-test}
fetch_spec_json()
{
    REPO=https://github.com/yamt/wasm-spec-test
    REF=527457c7d9bb3c2c80c26911351c602bd0f2f25e
    mkdir "${TEST_DIR}"
    git -C "${TEST_DIR}" init
    git -C "${TEST_DIR}" fetch --depth 1 ${REPO} ${REF}
    git -C "${TEST_DIR}" checkout FETCH_HEAD
}
test -d "${TEST_DIR}" || fetch_spec_json
