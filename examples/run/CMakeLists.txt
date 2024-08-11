cmake_minimum_required(VERSION 3.16)

include(../../cmake/LLVM.cmake)

project(run LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)

if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID STREQUAL GNU)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wvla -Werror")
endif()
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Wall /WX /wd4018 /wd4061 /wd4100 /wd4127 /wd4146 /wd4189 /wd4206 /wd4242 /wd4244 /wd4245 /wd4267 /wd4296 /wd4310 /wd4388 /wd4389 /wd4820 /wd4996")
endif()

find_package(toywasm-lib-core REQUIRED)

set(sources
	"run.c"
)

add_executable(run ${sources})
target_link_libraries(run toywasm-lib-core)
