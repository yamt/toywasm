#! /bin/sh

# usage:
# ./run-wast.sh v128.wast

set -e
set -x

THIS_DIR=$(cd $(dirname $0) && pwd -P)

TOYWASM_BUILD_DIR=${TOYWASM_BUILD_DIR:-${THIS_DIR}/../b}
TOYWASM=${TOYWASM:-${TOYWASM_BUILD_DIR}/toywasm}
SPECTEST_WASM=${SPECTEST_WASM:-${TOYWASM_BUILD_DIR}/spectest.wasm}

# https://github.com/yamt/wasm3/tree/toywasm-test
RUN_SPEC_TEST=${RUN_SPEC_TEST:-${THIS_DIR}/../.wasm3/test/run-spec-test.py}

WAST2JSON=${WAST2JSON:-wast2json}

TOYWASM_OPTIONS="--repl --repl-prompt wasm3"
# some spec tests expects overflows
#TOYWASM_OPTIONS="${TOYWASM_OPTIONS} --max-frames 2000 --max-stack-cells 10000"

TMP=$(mktemp -d)
WAST=${WAST:-$1}
WAST=$(basename ${WAST})
JSON=${TMP}/${WAST%.*}.json
${WAST2JSON} -o ${JSON} ${WAST}
${RUN_SPEC_TEST} \
--exec "${TOYWASM} ${TOYWASM_OPTIONS}" \
--spectest ${SPECTEST_WASM} \
--spec-dir ${TMP} \
${JSON}
rm -r ${TMP}
