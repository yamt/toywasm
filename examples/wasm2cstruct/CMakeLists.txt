cmake_minimum_required(VERSION 3.16)

include(../../cmake/LLVM.cmake)

project(wasm2cstruct LANGUAGES C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wvla -Werror")

find_package(toywasm-lib-core REQUIRED)

set(app_sources
	"main.c"
	"cstruct.c"
)

add_executable(wasm2cstruct ${app_sources})
target_link_libraries(wasm2cstruct toywasm::toywasm-lib-core m)
