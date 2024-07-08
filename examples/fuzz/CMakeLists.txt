cmake_minimum_required(VERSION 3.16)

include(../../cmake/LLVM.cmake)

project(fuzz-instantiate LANGUAGES CXX)

option(FUZZ_WASI "Enable WASI" OFF)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/fuzz_config.h.in"
	"${CMAKE_BINARY_DIR}/fuzz_config.h")
	include_directories(${CMAKE_BINARY_DIR})

set(SANITIZER_FLAGS "-fsanitize=fuzzer,address")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wvla -Werror ${SANITIZER_FLAGS}")

find_package(toywasm-lib-core REQUIRED)
if(FUZZ_WASI)
find_package(toywasm-lib-wasi REQUIRED)
endif()

set(app_sources
	"main.cxx"
)

add_executable(fuzz-instantiate ${app_sources})
target_link_libraries(fuzz-instantiate
	toywasm-lib-core
	$<$<BOOL:${FUZZ_WASI}>:toywasm-lib-wasi>
	m)
