#! /bin/sh

# Note: spidermonkey uses nan-boxing, which is one of motivations of
# nan-canonicalization in wasm.
# cf. https://github.com/WebAssembly/design/issues/1463

set -e
BIN=.spidermonkey/spidermonkey.wasm
if [ ! -f ${BIN} ]; then
    URL=https://registry-cdn.wapm.io/contents/mozilla/spidermonkey/0.0.1/build/spidermonkey.wasm
    mkdir -p .spidermonkey
    curl -L -o ${BIN} ${URL}
fi
OUT=$(mktemp)
ERROR=$(mktemp)
set +e
tr -d '\n' < test/pi.js | "$@" ${BIN} > ${OUT} 2> ${ERROR}
RESULT=$?
set -e
echo "stdout:"
cat ${OUT}
echo "stderr:"
cat ${ERROR}

# spidermonkey.wasm ends up with executing unreachable on EOF
test ${RESULT} -eq 1
grep -F "Error: [trap] unreachable executed" ${OUT}
# The sanitizer preserves the original exit status when it wasn't 0
# at least where _exit can be intercepted, including macOS.
# Check the stderr output to see if it complained something.
grep -E "(LeakSanitizer|AddressSanitizer)" ${ERROR} && false

# Check the result of pi.js
grep -F 3.1415 ${OUT}
rm ${OUT}
rm ${ERROR}
