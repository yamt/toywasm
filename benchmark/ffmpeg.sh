#! /bin/sh

set -e

# Note: macOS time command is assumed below
# https://github.com/apple-oss-distributions/shell_cmds/blob/main/time/time.c

run()
{
    echo "===== $1"
    shift 1
    echo "----- $@"
    OUTPUT=$(mktemp)
    ./test/run-ffmpeg.sh /usr/bin/time -l "$@" > ${OUTPUT} 2>&1
    grep -E "(real.*user.*sys|instructions retired|peak memory footprint|maximum resident set size)" ${OUTPUT}
    rm ${OUTPUT}
}

# Note: toywasm uses read-only mmap to load the wasm binary.
# it might be a bit unfair to compare the memory stats with engines
# which use malloc+read to load wasm binaries.

TOYWASM=${TOYWASM:-toywasm}
run "toywasm" ${TOYWASM} --wasi --wasi-dir .video --
# run "toywasm" ${TOYWASM} --wasi --wasi-dir .video --disable-jump-table --disable-localtype-cellidx --disable-resulttype-cellidx --

run "$(wasm3 --version|head -1)" wasm3 --dir .video --

run "$(iwasm.fast --version) (fast interpreter)" iwasm.fast --dir=.video
run "$(iwasm.classic --version) (classic interpreter)" iwasm.classic --dir=.video
# Note: i needed to tweak these size options manually to run
# this particular wasm binary
run "$(iwasm.fast-jit --version) (fast jit)" iwasm.fast-jit --dir=.video --jit-codecache-size=100000000

run "$(wasmer --version)" wasmer run --dir .video --
run "$(wasmtime --version)" wasmtime run --dir .video --
