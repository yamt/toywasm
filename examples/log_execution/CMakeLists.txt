cmake_minimum_required(VERSION 3.16)

include(../../cmake/LLVM.cmake)

project(runwasi LANGUAGES C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wvla -Werror")

find_package(toywasm-lib-core REQUIRED)
find_package(toywasm-lib-wasi REQUIRED)

# REVISIT: make runwasi.[ch] a library?

set(sources
	"../runwasi/runwasi.c"
	"../runwasi/runwasi_cli_args.c"
	"main.c"
	"log_execution.c"
)

add_executable(log-execution ${sources})
target_link_libraries(log-execution toywasm-lib-core toywasm-lib-wasi m)
target_include_directories(log-execution PRIVATE "../runwasi")
