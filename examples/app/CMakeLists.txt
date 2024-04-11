cmake_minimum_required(VERSION 3.16)

include(../../cmake/LLVM.cmake)

project(app LANGUAGES C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wvla -Werror")

find_package(toywasm-lib-core REQUIRED)

set(app_sources
	"app.c"
)

add_executable(app ${app_sources})
target_link_libraries(app toywasm-lib-core m)
