cmake_minimum_required(VERSION 3.16)
project(toy-wasm LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)

include(CTest)
include(CheckIPOSupported)

if(BUILD_TESTING)
enable_testing()
endif()

if(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "default" FORCE)
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

option(TOYWASM_USE_JUMP_BINARY_SEARCH "Enable binary search for jump tables" ON)
if(TOYWASM_USE_JUMP_BINARY_SEARCH)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_JUMP_BINARY_SEARCH")
endif()

option(TOYWASM_USE_JUMP_CACHE "Enable single-entry cache for jump tables" OFF)
if(TOYWASM_USE_JUMP_CACHE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_JUMP_CACHE")
endif()

option(TOYWASM_USE_JUMP_CACHE2 "Enable single-entry cache for jump tables" ON)
if(TOYWASM_USE_JUMP_CACHE2)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_JUMP_CACHE2")
endif()

option(TOYWASM_USE_LOCALS_CACHE "Enable current_locals" ON)
if(TOYWASM_USE_LOCALS_CACHE)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_LOCALS_CACHE")
endif()

option(TOYWASM_USE_SEPARATE_LOCALS "Separate locals and stack" ON)
if(TOYWASM_USE_SEPARATE_LOCALS)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUSE_SEPARATE_LOCALS")
endif()

# Prefer the homebrew version because xcode clang doesn't have detect_leaks
if(NOT DEFINED USE_LSAN)
set(USE_LSAN ON)
endif()
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
find_program(BREW_CLANG /usr/local/opt/llvm@13/bin/clang)
if(BREW_CLANG)
set(CMAKE_C_COMPILER ${BREW_CLANG})
else()
set(USE_LSAN OFF)
endif()
list(APPEND TEST_ENV "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp:print_suppressions=0")
list(APPEND TEST_ENV "MallocNanoZone=0")
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
if (CMAKE_C_COMPILER_ID MATCHES "Clang")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fshort-enums")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -fomit-frame-pointer")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wno-unknown-warning-option -Wno-unused-but-set-variable -Wno-unused-variable -Wno-return-type")

if(NOT DEFINED USE_ASAN)
set(USE_ASAN ON)
endif()
if(USE_ASAN)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")
endif()

if(USE_LSAN)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=leak")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=leak")
set(ASAN_DETECT_LEAKS 1)
else()
set(ASAN_DETECT_LEAKS 0)
endif()

if(NOT DEFINED USE_IPO)
check_ipo_supported(RESULT HAVE_IPO)
if (CMAKE_BUILD_TYPE MATCHES "Release")
if(HAVE_IPO)
set(USE_IPO ON)
else()
set(USE_IPO OFF)
endif()
else()
set(USE_IPO OFF)
endif()
endif()

list(APPEND TEST_ENV "ASAN_OPTIONS=detect_leaks=${ASAN_DETECT_LEAKS}:detect_stack_use_after_return=1")
list(APPEND TEST_ENV "UBSAN_OPTIONS=print_stacktrace=1")

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_C_COMPILER_ID: ${CMAKE_C_COMPILER_ID}")
message(STATUS "BUILD_TESTING: ${BUILD_TESTING}")
message(STATUS "USE_IPO: ${USE_IPO}")
message(STATUS "USE_ASAN: ${USE_ASAN}")
message(STATUS "USE_LSAN: ${USE_LSAN}")
message(STATUS "TOYWASM_USE_SEPARATE_EXECUTE: ${TOYWASM_USE_SEPARATE_EXECUTE}")
message(STATUS "TOYWASM_USE_TAILCALL: ${TOYWASM_USE_TAILCALL}")
message(STATUS "TOYWASM_ENABLE_TRACING: ${TOYWASM_ENABLE_TRACING}")
message(STATUS "TOYWASM_USE_JUMP_BINARY_SEARCH: ${TOYWASM_USE_JUMP_BINARY_SEARCH}")
message(STATUS "TOYWASM_USE_JUMP_CACHE: ${TOYWASM_USE_JUMP_CACHE}")
message(STATUS "TOYWASM_USE_JUMP_CACHE2: ${TOYWASM_USE_JUMP_CACHE2}")
message(STATUS "TOYWASM_USE_LOCALS_CACHE: ${TOYWASM_USE_LOCALS_CACHE}")
message(STATUS "TOYWASM_USE_SEPARATE_LOCALS: ${TOYWASM_USE_SEPARATE_LOCALS}")

# lib

set(lib_sources
	"bitmap.c"
	"context.c"
	"decode.c"
	"endian.c"
	"exec.c"
	"expr.c"
	"fileio.c"
	"host_instance.c"
	"import_object.c"
	"insn.c"
	"instance.c"
	"leb128.c"
	"module.c"
	"report.c"
	"repl.c"
	"type.c"
	"util.c"
	"validation.c"
	"vec.c"
	"wasi.c"
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
target_link_libraries(toywasm-cli toywasm-lib m)
set_target_properties(toywasm-cli PROPERTIES OUTPUT_NAME toywasm)

add_test(NAME toywasm-cli-simple-module COMMAND
	toywasm-cli --load spectest.wasm --invoke "print_i32 123"
)
set_tests_properties(toywasm-cli-simple-module PROPERTIES ENVIRONMENT "${TEST_ENV}")

add_test(NAME toywasm-cli-wasm3-spec-test
	COMMAND ./run-wasm3-spec-test-opam-2.0.0.sh --exec "${CMAKE_BINARY_DIR}/toywasm --repl --repl-prompt wasm3" --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test PROPERTIES ENVIRONMENT "${TEST_ENV}")

add_test(NAME toywasm-cli-wasm3-spec-test-disable-jump-table
	COMMAND ./run-wasm3-spec-test-opam-2.0.0.sh --exec "${CMAKE_BINARY_DIR}/toywasm --disable-jump-table --repl --repl-prompt wasm3" --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test-disable-jump-table PROPERTIES ENVIRONMENT "${TEST_ENV}")

# sample wasm files

if(BUILD_TESTING)
set(wat_files
	spectest.wat
)

find_program(WAT2WASM wat2wasm REQUIRED)
foreach(wat ${wat_files})
	get_filename_component(f ${wat} NAME_WLE)
	set(wasm "${f}.wasm")
	add_custom_command(OUTPUT ${wasm}
		COMMAND ${WAT2WASM} -o ${wasm} ${CMAKE_CURRENT_SOURCE_DIR}/${wat}
		MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${wat}
		COMMENT "Building ${wasm}")
	add_custom_target(build-${wasm} ALL DEPENDS ${wasm})
endforeach()
endif()

# unit test

if(BUILD_TESTING)
find_package(cmocka CONFIG REQUIRED)
include_directories(${CMOCKA_INCLUDE_DIR})

set(test_sources
	"test.c"
)

add_executable(toywasm-test ${test_sources})
target_link_libraries(toywasm-test toywasm-lib m ${CMOCKA_LIBRARY})

add_test(NAME toywasm-test COMMAND toywasm-test)
set_tests_properties(toywasm-test PROPERTIES ENVIRONMENT "${TEST_ENV}")
endif()
