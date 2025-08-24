#! /bin/sh

set -e

# Note: macOS time command is assumed below
# https://github.com/apple-oss-distributions/shell_cmds/blob/main/time/time.c

run()
{
    sync;sync;sync;sleep 10
    echo "===== $1"
    shift 1
    echo "----- $@"
    OUTPUT=$(mktemp)
    ./test/run-ffmpeg.sh /usr/bin/time -l "$@" > ${OUTPUT} 2>&1
    grep -E "(real.*user.*sys|instructions retired|peak memory footprint|maximum resident set size)" ${OUTPUT}
    rm ${OUTPUT}
}

# Remove on-disk cache for consistent results.
# XXX what's the proper way to clear cache?
wasmer cache clean
rm -rf ~/.wasmer/cache
rm -rf ~/Library/Caches/BytecodeAlliance.wasmtime

# Note: toywasm uses read-only mmap to load the wasm binary.
# it might be a bit unfair to compare the memory stats with engines
# which use malloc+read to load wasm binaries.

echo "+++++++++++ Interpreters +++++++++++"

TOYWASM=${TOYWASM:-toywasm}
run "$(${TOYWASM} --version | head -1) (default configuration)" ${TOYWASM} --wasi --wasi-dir .video --

# with fixed sized cells.
# separate binary as it's a build-time option.
if [ -n "${TOYWASM_FIXED}" ]; then
    run "$(${TOYWASM_FIXED} --version | head -1) (128-bit fixed cells)" ${TOYWASM_FIXED} --wasi --wasi-dir .video --
fi

if [ -n "${TOYWASM_FIXED_NOSIMD}" ]; then
    run "$(${TOYWASM_FIXED_NOSIMD} --version | head -1) (64-bit fixed cells, SIMD disabled)" ${TOYWASM_FIXED_NOSIMD} --wasi --wasi-dir .video --
fi

# without tables. optional because this is very slow.
if [ -n "${TOYWASM_SLOW}" ]; then
    run "$(${TOYWASM} --version | head -1) (annotations disabled, very slow)" ${TOYWASM} --wasi --wasi-dir .video --disable-jump-table --disable-localtype-cellidx --disable-resulttype-cellidx --
fi

run "$(wasm3 --version|head -1)" wasm3 --dir .video --

run "$(iwasm.fast --version|head -1) (fast interpreter)" iwasm.fast --dir=.video
run "$(iwasm.classic --version|head -1) (classic interpreter)" iwasm.classic --dir=.video

run "$(wasmedge --version|head -1) (interpreter)" wasmedge --dir .video --

run "$(wasmi_cli --version)" wasmi_cli --dir .video --

run "wazero $(wazero version) (interpreter)" wazero run -interpreter -mount .video --

# https://github.com/WebAssembly/wabt/issues/2074
#
# [mov,mp4,m4a,3gp,3g2,mj2 @ 0xb7e740] moov atom not found
# .video/video-1080p-60fps-2s.mp4: Invalid data found when processing input
#
# run "wasm-interp $(wasm-interp --version)" wasm-interp --wasi --dir .video --

echo "+++++++++++ JIT ++++++++++++++++++++"

# Note: i needed to tweak these size options manually to run
# this particular wasm binary
run "$(iwasm.fast-jit --version|head -1) (fast jit)" iwasm.fast-jit --dir=.video --jit-codecache-size=100000000

run "$(wasmer --version)" wasmer run --mapdir .video::.video --
run "$(wasmtime --version)" wasmtime run --mapdir .video::.video --

# XXX i'm not sure how compilation cache works by default.
run "wazero $(wazero version)" wazero run -mount .video --
