#! /bin/sh

set -e

export TEST_DIR=.wasm-spec-test
./test/fetch-spec-test.sh
exec ./test/run-wasm3-spec-test.sh --spec-dir $(pwd -P)/${TEST_DIR}/extended-const/test "$@"
