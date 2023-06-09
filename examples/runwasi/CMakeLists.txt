cmake_minimum_required(VERSION 3.16)

project(runwasi LANGUAGES C)

find_package(toywasm-lib-core REQUIRED)
find_package(toywasm-lib-wasi REQUIRED)

set(sources
	"runwasi.c"
)

add_executable(runwasi ${sources})
target_link_libraries(runwasi toywasm-lib-core toywasm-lib-wasi m)
