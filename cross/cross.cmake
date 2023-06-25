# usage:
#
# dpkg --add-architecture arm64
# apt update
# apt install -y crossbuild-essential-arm64 libcmocka-dev:arm64 wabt
# mkdir b
# cd b
# cmake -DCMAKE_TOOLCHAIN_FILE=../cross/cross.cmake ..

# https://cmake.org/cmake/help/book/mastering-cmake/chapter/Cross%20Compiling%20With%20CMake.html#toolchain-files

# Note for ubuntu
#
# crossbuild-essential-arm64 populate files under
# /usr/aarch64-linux-gnu.
# On the other hand, -dev packages seem to share /usr/include among archs.
# eg. libcmocka-dev:arm64 on amd64 looks like:
#
# 9bff2fafd65e# dpkg -L libcmocka-dev:arm64
# /.
# /usr
# /usr/include
# /usr/include/cmocka.h
# /usr/include/cmocka_pbc.h
# /usr/include/cmockery
# /usr/include/cmockery/cmockery.h
# /usr/include/cmockery/pbc.h
# /usr/lib
# /usr/lib/aarch64-linux-gnu
# /usr/lib/aarch64-linux-gnu/cmake
# /usr/lib/aarch64-linux-gnu/cmake/cmocka
# /usr/lib/aarch64-linux-gnu/cmake/cmocka/cmocka-config-version.cmake
# /usr/lib/aarch64-linux-gnu/cmake/cmocka/cmocka-config.cmake
# /usr/lib/aarch64-linux-gnu/pkgconfig
# /usr/lib/aarch64-linux-gnu/pkgconfig/cmocka.pc
# /usr/share
# /usr/share/doc
# /usr/share/doc/libcmocka-dev
# /usr/share/doc/libcmocka-dev/copyright
# /usr/lib/aarch64-linux-gnu/libcmocka.so
# /usr/lib/aarch64-linux-gnu/libcmockery.so
# /usr/share/doc/libcmocka-dev/changelog.Debian.gz
# 9bff2fafd65e# 
#
# https://wiki.ubuntu.com/MultiarchCross
#
# REVISIT: is it expected to use PKG_CONFIG_LIBDIR?
# eg. PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig/

set(TRIPLET ${TRIPLET} CACHE STRING "aarch64-linux-gnu" FORCE)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER_TARGET ${TRIPLET})
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER_TARGET ${TRIPLET})
set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_AR /usr/bin/llvm-ar)
set(CMAKE_C_COMPILER_AR ${CMAKE_AR})
set(CMAKE_RANLIB /usr/bin/llvm-ranlib)
set(CMAKE_C_COMPILER_RANLIB ${CMAKE_RANLIB})

# # dpkg -L libstdc++-9-dev-arm64-cross|grep -F c++config
# /usr/aarch64-linux-gnu/include/c++/9/aarch64-linux-gnu/bits/c++config.h
# #
# REVISIT: is there a nicer ways to make clang find them?
string(APPEND CMAKE_C_FLAGS_INIT " -isystem /usr/${TRIPLET}/include")
string(APPEND CMAKE_C_FLAGS_INIT " -isystem /usr/${TRIPLET}/include/c++/9/${TRIPLET}")
string(APPEND CMAKE_CXX_FLAGS_INIT " -isystem /usr/${TRIPLET}/include")
string(APPEND CMAKE_CXX_FLAGS_INIT " -isystem /usr/${TRIPLET}/include/c++/9/${TRIPLET}")

set(CMAKE_FIND_ROOT_PATH /usr/${TRIPLET})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${ARCH})
