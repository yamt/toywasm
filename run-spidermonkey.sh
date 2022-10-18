#! /bin/sh

set -e
BIN=.spidermonkey/spidermonkey.wasm
if [ ! -f ${BIN} ]; then
    URL=https://registry-cdn.wapm.io/contents/mozilla/spidermonkey/0.0.1/build/spidermonkey.wasm
	mkdir -p .spidermonkey
    curl -L -o ${BIN} ${URL}
fi
tr -d '\n' < pi.js | "$@" ${BIN} | grep -F 3.1415
