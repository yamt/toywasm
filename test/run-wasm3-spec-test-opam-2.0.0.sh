#! /bin/sh

set -e

./test/fetch-spec-test.sh
exec ./test/run-wasm3-spec-test.sh --spec-dir $(pwd -P)/${TEST_DIR}/test "$@"
