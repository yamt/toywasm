This directory contains a few scripts to cross build toywasm
on ubuntu-focal/amd64.

We avoid making this too toywasm-specific so that it can be
useful to cross build other cmake-based software as well.

I usually use this with a docker image built from
https://github.com/yamt/garbage/tree/master/myubuntu.

Our github CI jobs also use these scripts.

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
