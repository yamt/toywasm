cmake_minimum_required(VERSION 3.16)

include(../../cmake/LLVM.cmake)

project(callgraph LANGUAGES C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wvla -Werror")

find_package(toywasm-lib-core REQUIRED)

set(app_sources
	"main.c"
	"callgraph.c"
)

add_executable(callgraph ${app_sources})
target_link_libraries(callgraph toywasm-lib-core m)
