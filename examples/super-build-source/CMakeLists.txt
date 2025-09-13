# An example to build toywasm library and application using it
# with ExternalProject.

cmake_minimum_required(VERSION 3.16)

project(app LANGUAGES)

set(toywasm_source_dir ${CMAKE_SOURCE_DIR}/../..)
set(toywasm_tmp_install ${CMAKE_CURRENT_BINARY_DIR}/install)

include(ExternalProject)
ExternalProject_Add(toywasm
    SOURCE_DIR ${toywasm_source_dir}
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${toywasm_tmp_install}
        -DBUILD_TESTING=OFF
        -DTOYWASM_BUILD_CLI=OFF
        -DTOYWASM_BUILD_UNITTEST=OFF
        -DTOYWASM_ENABLE_WASI=OFF
        -DTOYWASM_ENABLE_WASM_SIMD=OFF
        -DTOYWASM_USE_SHORT_ENUMS=OFF
        -DTOYWASM_USE_SMALL_CELLS=OFF
    BUILD_COMMAND
        cmake --build .
    INSTALL_COMMAND
        cmake --build . --target install
)

ExternalProject_Add(app
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/app
    CMAKE_ARGS
        -DCMAKE_PREFIX_PATH=${toywasm_tmp_install}
    INSTALL_COMMAND ""
)

add_dependencies(app toywasm)
