cmake_minimum_required(VERSION 3.16)

include(../../cmake/LLVM.cmake)

project(run LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)

if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID STREQUAL GNU)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wvla -Werror")
endif()
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Wall /WX")
endif()

find_package(toywasm-lib-core REQUIRED)

set(sources
	"run.c"
)

add_executable(run ${sources})
target_link_libraries(run toywasm-lib-core)
