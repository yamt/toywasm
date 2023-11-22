cmake_minimum_required(VERSION 3.16)

include(../../cmake/HomebrewLLVM.cmake)

project(runwasi LANGUAGES C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wvla -Werror")

find_package(toywasm-lib-core REQUIRED)
find_package(toywasm-lib-wasi REQUIRED)

set(sources
	"runwasi.c"
	"runwasi_cli_args.c"
	"main.c"
)

add_executable(runwasi ${sources})
target_link_libraries(runwasi toywasm-lib-core toywasm-lib-wasi m)
