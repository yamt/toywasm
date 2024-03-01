#! /bin/sh

set -x
set -e

test -e littlefs.img || ./extract.sh

TOYWASM=${TOYWASM:-${TEST_RUNTIME_EXE:-toywasm}}
DIR=$(mktemp -d)
TESTFILE=cp.wasm
cp ${TESTFILE} ${DIR}/testfile1

# copy a file from host to littlefs
${TOYWASM} \
--wasi \
--wasi-dir=${DIR}::host \
--wasi-littlefs-dir=littlefs.img::.::lfs \
-- \
cp.wasm host/testfile1 lfs/testfile2

# copy a file from littlefs to host
${TOYWASM} \
--wasi \
--wasi-dir=${DIR}::host \
--wasi-littlefs-dir=littlefs.img::.::lfs \
-- \
cp.wasm lfs/testfile2 host/testfile3

# confirm the result
diff ${TESTFILE} ${DIR}/testfile3

rm -r ${DIR}
