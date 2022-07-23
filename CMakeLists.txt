cmake_minimum_required(VERSION 3.23)
project(toy-wasm)

include(CTest)
include(CheckIPOSupported)

if(BUILD_TESTING)
enable_testing()
endif()

if(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE "Release")
endif()

option(TOYWASM_USE_SEPARATE_EXECUTE "Use separate execute callback" ON)
if (TOYWASM_USE_SEPARATE_EXECUTE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_SEPARATE_EXECUTE")
endif()

option(TOYWASM_USE_TAILCALL "Use tailcall" ON)
if (TOYWASM_USE_TAILCALL)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_TAILCALL")
endif()

option(TOYWASM_ENABLE_TRACING "Enable xlog_trace" OFF)
if (TOYWASM_ENABLE_TRACING)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_TRACING")
endif()

# Use the homebrew version because xcode clang doesn't have detect_leaks
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
set(CMAKE_C_COMPILER /usr/local/opt/llvm@13/bin/clang)
endif ()

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -fomit-frame-pointer")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unused-but-set-variable -Wno-unused-variable -Wno-return-type")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address -fsanitize=leak -g")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address -fsanitize=leak -g")

if (CMAKE_BUILD_TYPE MATCHES "Release")
check_ipo_supported(RESULT USE_IPO)
endif()

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(STATUS "BUILD_TESTING: ${BUILD_TESTING}")
message(STATUS "USE_IPO: ${USE_IPO}")
message(STATUS "TOYWASM_USE_SEPARATE_EXECUTE: ${TOYWASM_USE_SEPARATE_EXECUTE}")
message(STATUS "TOYWASM_USE_TAILCALL: ${TOYWASM_USE_TAILCALL}")
message(STATUS "TOYWASM_ENABLE_TRACING: ${TOYWASM_ENABLE_TRACING}")

# lib

set(lib_sources
	"context.c"
	"decode.c"
	"endian.c"
	"exec.c"
	"expr.c"
	"fileio.c"
	"import_object.c"
	"insn.c"
	"instance.c"
	"leb128.c"
	"module.c"
	"repl.c"
	"type.c"
	"util.c"
	"validation.c"
	"vec.c"
	"xlog.c"
)

add_library(toywasm-lib STATIC ${lib_sources})
set_target_properties(toywasm-lib PROPERTIES OUTPUT_NAME toywasm)
if (USE_IPO)
# Note: -flto=full seems to produce considerably faster code
# than flto=thin for us. However, cmake INTERPROCEDURAL_OPTIMIZATION
# always use -flto=thin for clang.
# cf. https://gitlab.kitware.com/cmake/cmake/-/issues/16808
set_property(TARGET toywasm-lib PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# cli

set(cli_sources
	"main.c"
	"repl.c"
)

add_executable(toywasm-cli ${cli_sources})
target_link_libraries(toywasm-cli toywasm-lib)
set_target_properties(toywasm-cli PROPERTIES OUTPUT_NAME toywasm)

add_test(NAME toywasm-cli-simple-module COMMAND
	toywasm-cli --load ../spectest.wasm --invoke "print_i32 123"
)

# unit test

if(BUILD_TESTING)
find_package(cmocka CONFIG REQUIRED)
include_directories(${CMOCKA_INCLUDE_DIR})

set(test_sources
	"test.c"
)

add_executable(toywasm-test ${test_sources})
target_link_libraries(toywasm-test toywasm-lib ${CMOCKA_LIBRARY})

add_test(NAME toywasm-test COMMAND toywasm-test)
endif()
