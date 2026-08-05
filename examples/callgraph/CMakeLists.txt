cmake_minimum_required(VERSION 3.16)

include(../../cmake/LLVM.cmake)

project(callgraph LANGUAGES C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wvla -Werror")

find_package(toywasm-lib-core REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JANSSON REQUIRED jansson)

set(app_sources
	"main.c"
	"callgraph.c"
	"jsonutil.c"
)

add_executable(callgraph ${app_sources})
target_include_directories(callgraph PRIVATE ${JANSSON_INCLUDE_DIRS})
target_link_directories(callgraph PRIVATE ${JANSSON_LIBRARY_DIRS})
target_link_libraries(callgraph toywasm::toywasm-lib-core m ${JANSSON_LIBRARIES})
