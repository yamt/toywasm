#! /bin/sh

CC="/opt/wasi-sdk/bin/clang -Os"

${CC} \
-o test.wasm \
test.c

${CC} \
-o test.host-func1.wasm \
-DUSE_HOST_LOAD \
test.c

${CC} -Xlinker --export-table \
-o test.host-func2.wasm \
-DUSE_HOST_LOAD_CALL \
test.c

${CC} -Xlinker --export-table \
-o test.host-func3.wasm \
-DUSE_HOST_LOAD \
-DUSE_HOST_LOAD_CALL \
test.c

${CC} -Xlinker --export-table \
-o test.host-func4.wasm \
-DUSE_HOST_LOAD_CALL_ADD \
test.c

${CC} -Xlinker --export-table \
-o test.host-func5.wasm \
-DUSE_HOST_LOAD \
-DUSE_HOST_LOAD_CALL_ADD \
test.c

${CC} -Xlinker --export-table \
-o test.host-func6.wasm \
-DUSE_HOST_LOAD_CALL \
-DUSE_HOST_LOAD_CALL_ADD \
test.c

${CC} -Xlinker --export-table \
-o test.host-func7.wasm \
-DUSE_HOST_LOAD \
-DUSE_HOST_LOAD_CALL \
-DUSE_HOST_LOAD_CALL_ADD \
test.c
