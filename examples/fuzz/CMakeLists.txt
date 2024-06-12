cmake_minimum_required(VERSION 3.16)

include(../../cmake/LLVM.cmake)

project(fuzz-instantiate LANGUAGES CXX)

set(SANITIZER_FLAGS "-fsanitize=fuzzer,address")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wvla -Werror ${SANITIZER_FLAGS}")

find_package(toywasm-lib-core REQUIRED)

set(app_sources
	"main.cxx"
)

add_executable(fuzz-instantiate ${app_sources})
target_link_libraries(fuzz-instantiate toywasm-lib-core m)
