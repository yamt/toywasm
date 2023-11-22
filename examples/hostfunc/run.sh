#! /bin/sh

set -e

for x in wasm/*.wasm; do
    echo
    echo "# $x"
    ./build/build-app/hostfunc $x
done
