This directory contains a few scripts to cross build toywasm
on ubuntu-focal/amd64.

I usually use this with a docker image built from
https://github.com/yamt/garbage/tree/master/myubuntu.

Also used by toywasm github CI jobs.

Expected usage:
```
./cross/setup-focal-proposed.sh
export ARCH=arm64
./cross/cross-setup.sh
./cross/setup-alternatives.sh

rm -rf build.cross.${ARCH}
./cross/cross-cmake-configure.sh

cd build.cross.${ARCH}
make
```
