#! /bin/sh

set -e

run() {
    printf "$1,"
    shift
    OUTPUT=$(mktemp)

    sync;sync;sync

    # ffmpeg binary downloaded by ../test/run-ffmpeg.sh
    /usr/bin/time -l $@ ../.ffmpeg/ffmpeg.wasm -version > ${OUTPUT} 2>&1

    # sanity checks
    grep -F "ffmpeg version" ${OUTPUT} > /dev/null
    grep -v "(Unrecognized|usage)" ${OUTPUT} > /dev/null

    grep -E "(real.*user.*sys|maximum resident set size)" ${OUTPUT} | \
    sed \
    -e 's/ *\([0-9][\.0-9]*\) real *\([0-9][\.0-9]*\) user *\([0-9][\.0-9]*\) sys */\1,\2,\3,/' \
    -e 's/ *\([0-9][0-9]*\) *maximum resident set size/\1/' | \
    tr -d '\n'
    echo
    rm ${OUTPUT}
}

wasmer cache clean 2> /dev/null
rm -rf ~/.wasmer/cache
rm -rf ~/Library/Caches/BytecodeAlliance.wasmtime
sync;sync;sync;sleep 3

run "toywasm (default)" "../b/toywasm --wasi --"
run "toywasm (no annotations)" "../b/toywasm --disable-jump-table --disable-localtype-cellidx --disable-resulttype-cellidx --wasi --"
run "wasm3 (default)" wasm3
run "wasm3 (no lazy)" wasm3 --compile
run "wamr (classic interp)" iwasm.classic
run "wamr (fast interp)" iwasm.fast
run "wasmi" "wasmi_cli --"
run "wazero (interp)" "wazero run -interpreter --"
run "wasmedge (interp)" "wasmedge --"

run "wamr (fast jit)" "iwasm.fast-jit --jit-codecache-size=100000000"
run "wamr (fast jit no lazy)" "iwasm.fast-jit-nolazy --jit-codecache-size=100000000"
run "wasmer (first run)" "wasmer run --"
run "wasmer (cached)" "wasmer run --"
run "wasmtime (first run)" "wasmtime run --"
run "wasmtime (cached)" "wasmtime run --"
