#! /bin/sh

# eg.
# ./test/run-ffmpeg.sh ./b/toywasm --wasi --wasi-dir .video --
# ./test/run-ffmpeg.sh wasm3 --dir .video --
# ./test/run-ffmpeg.sh iwasm --dir=.video
# ./test/run-ffmpeg.sh wasmtime run --dir .video --
# ./test/run-ffmpeg.sh wasmer run --dir .video --
# ./test/run-ffmpeg.sh wasm-interp --wasi --dir .video --
#
# ffmpeg uses color output only for wasm3 because, when using uvwasi,
# wasm3 provides some environment variables by default.
# cf. https://github.com/wasm3/wasm3/blob/011597ecea739399daa36f1a138b9b5e18672d6d/source/m3_api_uvwasi.c#L1107
# You can get a similar output with:
# eg.
# ./test/run-ffmpeg.sh wasmtime run --env TERM=xterm-256color --dir .video --
#
# for some reasons, wasm-interp doesn't work:
#
# [mov,mp4,m4a,3gp,3g2,mj2 @ 0xb7e740] Format mov,mp4,m4a,3gp,3g2,mj2 detected only with low score of 1, misdetection possible!
# [mov,mp4,m4a,3gp,3g2,mj2 @ 0xb7e740] moov atom not found
# .video/video-1080p-60fps-2s.mp4: Invalid data found when processing input
#
# for some reasons, WAMR fast-jit doesn't work:
#
# % ./test/run-ffmpeg.sh iwasm --dir=.video
# fast jit compilation failed: Error: allocate memory failed
# WASM module load failed: fast jit compilation failed
# %
# % ./test/run-ffmpeg.sh iwasm --jit-codecache-size=20000000 --dir=.video
# 0x1000fast jit compilation failed: unknown error
# WASM module load failed: fast jit compilation failed
# %

set -e

checkfile() {
	FILE=$1
    HASH=$2
    test "$(openssl sha256 -r ${FILE} | cut -d' '  -f1)" = ${HASH}
}

fetch() {
    BIN=$1
    HASH=$2
    URL=$3
    if ! checkfile ${BIN} ${HASH}; then
        mkdir -p $(dirname ${BIN})
        curl -L -o ${BIN} ${URL}
    fi
}

# https://runno.dev/articles/ffmpeg
# https://github.com/SebastiaanYN/FFmpeg-WASI
fetch .ffmpeg/ffmpeg.wasm \
9d045073c496cca32285a3aaab95e9b90ee0de36aef62648aa3b85b432171fbc \
https://assets.runno.dev/ffmpeg/ffmpeg.wasm
fetch .video/video-1080p-60fps-2s.mp4 \
a703d46c89839d34480e567a2e86b74ada7c1077df8e635c282386d7a6e6b044 \
https://github.com/SebastiaanYN/FFmpeg-WASI/raw/4218a47f117df5f900bb2b9bda4f100aa98d1bd7/videos/video-1080p-60fps-2s.mp4

OUTDIR=$(mktemp -d .video/out.XXXXXX)

"$@" .ffmpeg/ffmpeg.wasm \
-i .video/video-1080p-60fps-2s.mp4 \
-ss 1 -vframes 1 \
${OUTDIR}/out-%03d.png

checkfile ${OUTDIR}/out-001.png \
f53da2af97aa7149809bdc6ee290ea66049650602d0da7cebf09225166de8884

rm -rf ${OUTDIR}
