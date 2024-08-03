cmake_minimum_required(VERSION 3.16)

include(../../cmake/LLVM.cmake)

project(runwasi_cstruct LANGUAGES C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wvla -Werror")

find_package(toywasm-lib-core REQUIRED)
find_package(toywasm-lib-wasi REQUIRED)

# REVISIT: make runwasi.[ch] a library?

set(sources
	"../runwasi/runwasi.c"
	"../runwasi/runwasi_cli_args.c"
	"main.c"
	"module.c"
)

add_executable(runwasi_cstruct ${sources})
target_link_libraries(runwasi_cstruct toywasm-lib-core toywasm-lib-wasi m)
target_include_directories(runwasi_cstruct PRIVATE "../runwasi")
