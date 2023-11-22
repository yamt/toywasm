#! /bin/sh

CC=/opt/wasi-sdk/bin/clang

${CC} -Xlinker --export-table \
-o test.wasm \
test.c

${CC} -Xlinker --export-table \
-o test.host-func1.wasm \
-DUSE_HOST_LOAD_CALL_ADD \
test.c

${CC} -Xlinker --export-table \
-o test.host-func2.wasm \
-DUSE_HOST_LOAD_CALL \
-DUSE_HOST_LOAD_CALL_ADD \
test.c

${CC} -Xlinker --export-table \
-o test.host-func3.wasm \
-DUSE_HOST_LOAD \
-DUSE_HOST_LOAD_CALL \
-DUSE_HOST_LOAD_CALL_ADD \
test.c
