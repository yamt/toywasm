#! /bin/sh

if [ ! -d wasi-shlib-bin ]; then
    curl -L https://github.com/yamt/garbage/releases/download/wasi-shlib-bin-20250824/wasi-shlib-bin.tgz | pax -rvz
fi
cd wasi-shlib-bin
exec ./test.sh
