#! /bin/sh

# run js.wasm

# expected usage:
#
# ./test/js-wasm.sh test/pi2.js
#
# TEST_RUNTIME_EXE="./test/toywasm-on-toywasm.py --wasi --wasi-dir=test --" ./test/js-wasm.sh test/pi2.js
# TEST_RUNTIME_EXE="iwasm --dir=test" ./test/js-wasm.sh test/pi2.js

set -e

# download js.wasm, which is mozilla's CI artifact.
# references:
# https://github.com/mozilla-spidermonkey/sm-wasi-demo

JSON=.js.wasm/data.json
if [ ! -f ${JSON} ]; then
    JSON_URL=https://raw.githubusercontent.com/mozilla-spidermonkey/sm-wasi-demo/main/data.json
    mkdir -p .js.wasm
    curl -L -o ${JSON} ${JSON_URL}
fi

BIN=.js.wasm/js.wasm
if [ ! -f ${BIN} ]; then
    URL=$(python3 test/js_wasm_url.py ${JSON} mozilla-release)
    mkdir -p .js.wasm
    curl -L -o ${BIN}.gz ${URL}
    gunzip ${BIN}.gz
fi

TEST_RUNTIME_EXE=${TEST_RUNTIME_EXE:-"toywasm --wasi --wasi-dir=test --"}
${TEST_RUNTIME_EXE} ${BIN} "$@"
