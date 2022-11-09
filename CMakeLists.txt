cmake_minimum_required(VERSION 3.16)

if(NOT CMAKE_TOOLCHAIN_FILE)
# Prefer the homebrew version because xcode clang doesn't have detect_leaks
# Note: CMAKE_SYSTEM_NAME is not available yet.
set(BREW_LLVM_HOME /usr/local/opt/llvm@13)
find_program(BREW_CLANG ${BREW_LLVM_HOME}/bin/clang)
if(BREW_CLANG)
set(CMAKE_C_COMPILER ${BREW_CLANG} CACHE FILEPATH "brew llvm clang" FORCE)
find_program(BREW_AR ${BREW_LLVM_HOME}/bin/llvm-ar REQUIRED)
set(CMAKE_AR ${BREW_AR} CACHE FILEPATH "brew llvm ar" FORCE)
set(CMAKE_C_COMPILER_AR ${BREW_AR} CACHE FILEPATH "brew llvm ar" FORCE)
find_program(BREW_RANLIB ${BREW_LLVM_HOME}/bin/llvm-ranlib REQUIRED)
set(CMAKE_RANLIB ${BREW_RANLIB} CACHE FILEPATH "brew llvm ar" FORCE)
set(CMAKE_C_COMPILER_RANLIB ${BREW_RANLIB} CACHE FILEPATH "brew llvm ranlib" FORCE)
endif()
endif()

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
option(TOYWASM_USE_TAILCALL "Use tailcall" ON)
option(TOYWASM_ENABLE_TRACING "Enable xlog_trace" OFF)
option(TOYWASM_USE_JUMP_BINARY_SEARCH "Enable binary search for jump tables" ON)
option(TOYWASM_USE_JUMP_CACHE "Enable single-entry cache for jump tables" OFF)
if(NOT TOYWASM_JUMP_CACHE2_SIZE)
set(TOYWASM_JUMP_CACHE2_SIZE 4 CACHE STRING "The size of jump cache")
endif()
option(TOYWASM_USE_LOCALS_CACHE "Enable current_locals" ON)
option(TOYWASM_USE_SEPARATE_LOCALS "Separate locals and stack" ON)
option(TOYWASM_USE_SMALL_CELLS "Use smaller stack cells" ON)
option(TOYWASM_USE_RESULTTYPE_CELLIDX "Index local lookup for resulttype" ON)
option(TOYWASM_USE_LOCALTYPE_CELLIDX "Index local lookup for localtype" ON)
option(TOYWASM_ENABLE_WRITER "Enable module writer" ON)

if(NOT DEFINED USE_LSAN)
set(USE_LSAN ON)
endif()
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
if(NOT BREW_CLANG)
set(USE_LSAN OFF)
endif()
list(APPEND TEST_ENV "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/test/lsan.supp:print_suppressions=0")
list(APPEND TEST_ENV "MallocNanoZone=0")
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
if (CMAKE_C_COMPILER_ID MATCHES "Clang")
# lld doesn't seem to support s390
# ld.lld: error: unknown emulation: elf64_s390
# https://github.com/llvm/llvm-project/blob/93b7bdcda7072581ef3f5ceaae8c4f0d549a0845/lld/ELF/Driver.cpp#L142-L166
if(NOT TRIPLET MATCHES "s390")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
else()
set(USE_IPO OFF)
endif()
endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fshort-enums")
if(CMAKE_C_COMPILER_ID STREQUAL GNU)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")
endif()

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fomit-frame-pointer")

if (NOT CMAKE_BUILD_TYPE MATCHES "Debug")
# Note: Release build disables assertions and thus yields a lot of
# used variables. We are not interested in fixing them.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-warning-option -Wno-unused-but-set-variable -Wno-unused-variable -Wno-return-type")
endif()

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
if (NOT CMAKE_BUILD_TYPE MATCHES "Debug")
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
message(STATUS "CMAKE_AR: ${CMAKE_AR}")
message(STATUS "CMAKE_C_COMPILER_AR: ${CMAKE_C_COMPILER_AR}")
message(STATUS "CMAKE_RANLIB: ${CMAKE_RANLIB}")
message(STATUS "CMAKE_C_COMPILER_RANLIB: ${CMAKE_C_COMPILER_RANLIB}")
message(STATUS "BUILD_TESTING: ${BUILD_TESTING}")
message(STATUS "USE_IPO: ${USE_IPO}")
message(STATUS "USE_ASAN: ${USE_ASAN}")
message(STATUS "USE_LSAN: ${USE_LSAN}")
message(STATUS "TOYWASM_USE_SEPARATE_EXECUTE: ${TOYWASM_USE_SEPARATE_EXECUTE}")
message(STATUS "TOYWASM_USE_TAILCALL: ${TOYWASM_USE_TAILCALL}")
message(STATUS "TOYWASM_ENABLE_TRACING: ${TOYWASM_ENABLE_TRACING}")
message(STATUS "TOYWASM_USE_JUMP_BINARY_SEARCH: ${TOYWASM_USE_JUMP_BINARY_SEARCH}")
message(STATUS "TOYWASM_USE_JUMP_CACHE: ${TOYWASM_USE_JUMP_CACHE}")
message(STATUS "TOYWASM_JUMP_CACHE2_SIZE: ${TOYWASM_JUMP_CACHE2_SIZE}")
message(STATUS "TOYWASM_USE_LOCALS_CACHE: ${TOYWASM_USE_LOCALS_CACHE}")
message(STATUS "TOYWASM_USE_SEPARATE_LOCALS: ${TOYWASM_USE_SEPARATE_LOCALS}")
message(STATUS "TOYWASM_USE_SMALL_CELLS: ${TOYWASM_USE_SMALL_CELLS}")
message(STATUS "TOYWASM_USE_RESULTTYPE_CELLIDX: ${TOYWASM_USE_RESULTTYPE_CELLIDX}")
message(STATUS "TOYWASM_USE_LOCALTYPE_CELLIDX: ${TOYWASM_USE_LOCALTYPE_CELLIDX}")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/lib/toywasm_config.h.in"
	"${CMAKE_BINARY_DIR}/toywasm_config.h")
include_directories(${CMAKE_BINARY_DIR})

# lib

set(lib_sources
	"lib/bitmap.c"
	"lib/cell.c"
	"lib/context.c"
	"lib/decode.c"
	"lib/endian.c"
	"lib/exec.c"
	"lib/expr.c"
	"lib/fileio.c"
	"lib/host_instance.c"
	"lib/import_object.c"
	"lib/insn.c"
	"lib/instance.c"
	"lib/leb128.c"
	"lib/module.c"
	"lib/report.c"
	"lib/type.c"
	"lib/util.c"
	"lib/validation.c"
	"lib/vec.c"
	"lib/wasi.c"
	"lib/xlog.c"
)

if(TOYWASM_ENABLE_WRITER)
set(lib_sources_writer
	"lib/module_writer.c"
)
endif()

add_library(toywasm-lib STATIC ${lib_sources} ${lib_sources_writer})
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
	"cli/main.c"
	"cli/repl.c"
)

add_executable(toywasm-cli ${cli_sources})
target_include_directories(toywasm-cli PRIVATE lib)
target_link_libraries(toywasm-cli toywasm-lib m)
set_target_properties(toywasm-cli PROPERTIES OUTPUT_NAME toywasm)

add_test(NAME toywasm-cli-simple-module COMMAND
	toywasm-cli --load spectest.wasm --invoke "print_i32 123"
)
set_tests_properties(toywasm-cli-simple-module PROPERTIES ENVIRONMENT "${TEST_ENV}")

add_test(NAME toywasm-cli-wasm3-spec-test
	COMMAND ./test/run-wasm3-spec-test-opam-2.0.0.sh --exec "${CMAKE_BINARY_DIR}/toywasm --repl --repl-prompt wasm3" --timeout 60 --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test PROPERTIES ENVIRONMENT "${TEST_ENV}")

add_test(NAME toywasm-cli-wasmtime-wasi-tests
	COMMAND ./test/run-wasmtime-wasi-tests.sh "${CMAKE_BINARY_DIR}/toywasm --wasi --wasi-dir ."
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasmtime-wasi-tests PROPERTIES ENVIRONMENT "${TEST_ENV}")

add_test(NAME toywasm-cli-spidermonkey
	COMMAND ./test/run-spidermonkey.sh ${CMAKE_BINARY_DIR}/toywasm --wasi
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-spidermonkey PROPERTIES ENVIRONMENT "${TEST_ENV}")

add_test(NAME toywasm-cli-wasm3-spec-test-disable-jump-table
	COMMAND ./test/run-wasm3-spec-test-opam-2.0.0.sh --exec "${CMAKE_BINARY_DIR}/toywasm --disable-jump-table --repl --repl-prompt wasm3" --timeout 60 --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test-disable-jump-table PROPERTIES ENVIRONMENT "${TEST_ENV}")

add_test(NAME toywasm-cli-wasm3-wasi-test
	COMMAND ./test/run-wasm3-wasi-test.sh --exec "${CMAKE_BINARY_DIR}/toywasm --wasi --wasi-dir ." --separate-args --timeout 360
	CONFIGURATIONS Release
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-wasi-test PROPERTIES ENVIRONMENT "${TEST_ENV}")

# sample wasm files

if(BUILD_TESTING)
set(wat_files
	test/spectest.wat
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
	"test/test.c"
)

add_executable(toywasm-test ${test_sources})
target_include_directories(toywasm-test PRIVATE lib)
target_link_libraries(toywasm-test toywasm-lib m ${CMOCKA_LIBRARY})

add_test(NAME toywasm-test COMMAND toywasm-test)
set_tests_properties(toywasm-test PROPERTIES ENVIRONMENT "${TEST_ENV}")
endif()
