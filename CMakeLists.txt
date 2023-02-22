cmake_minimum_required(VERSION 3.16)

include(cmake/HomebrewLLVM.cmake)

project(toywasm LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)
# we need timespec (199309L), flockfile (199506L), etc
add_compile_definitions(_POSIX_C_SOURCE=199506L)

include(CTest)
include(CheckIPOSupported)

if(BUILD_TESTING)
enable_testing()
endif()

include(cmake/ToywasmConfig.cmake)

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
	"lib/idalloc.c"
	"lib/import_object.c"
	"lib/insn.c"
	"lib/instance.c"
	"lib/leb128.c"
	"lib/list.c"
	"lib/module.c"
	"lib/nbio.c"
	"lib/report.c"
	"lib/timeutil.c"
	"lib/type.c"
	"lib/util.c"
	"lib/validation.c"
	"lib/vec.c"
	"lib/wasi.c"
	"lib/xlog.c"
	"${CMAKE_BINARY_DIR}/toywasm_config.c"
)

if(TOYWASM_ENABLE_WASM_THREADS)
list(APPEND lib_sources
	"lib/cluster.c"
	"lib/waitlist.c")
if(TOYWASM_USE_USER_SCHED)
list(APPEND lib_sources
	"lib/usched.c")
else()
list(APPEND lib_sources
	"lib/lock.c")
endif()
if(TOYWASM_ENABLE_WASI_THREADS)
list(APPEND lib_sources
	"lib/shared_memory.c"
	"lib/wasi_threads.c")
endif()
endif()

if(TOYWASM_ENABLE_WRITER)
set(lib_sources_writer
	"lib/module_writer.c"
)
endif()

set(lib_headers
	"lib/bitmap.h"
	"lib/cell.h"
	"lib/context.h"
	"lib/fileio.h"
	"lib/instance.h"
	"lib/list.h"
	"lib/load_context.h"
	"lib/lock.h"
	"lib/module.h"
	"lib/module_writer.h"
	"lib/nbio.h"
	"lib/options.h"
	"lib/platform.h"
	"lib/report.h"
	"lib/type.h"
	"lib/usched.h"
	"lib/vec.h"
	"lib/wasi.h"
	"lib/wasi_threads.h"
	"lib/xlog.h"
	"${CMAKE_BINARY_DIR}/toywasm_config.h"
	"${CMAKE_BINARY_DIR}/toywasm_version.h"
)

add_library(toywasm-lib STATIC ${lib_sources} ${lib_sources_writer})
set_target_properties(toywasm-lib PROPERTIES OUTPUT_NAME toywasm)
if (USE_IPO)
# Note: -flto=full seems to produce considerably faster code
# than flto=thin for us. However, cmake INTERPROCEDURAL_OPTIMIZATION
# always use -flto=thin for clang.
# cf. https://gitlab.kitware.com/cmake/cmake/-/issues/16808
set_property(TARGET toywasm-lib PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
set_property(TARGET toywasm-lib PROPERTY PUBLIC_HEADER ${lib_headers})
target_include_directories(toywasm-lib
                           INTERFACE
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib>
                           $<INSTALL_INTERFACE:include>)

install(TARGETS toywasm-lib
        EXPORT toywasm-config
        PUBLIC_HEADER DESTINATION include/toywasm)
install(EXPORT toywasm-config
        DESTINATION lib/cmake/toywasm)

# cli

set(cli_sources
	"cli/main.c"
	"cli/repl.c"
)

add_executable(toywasm-cli ${cli_sources})
target_link_libraries(toywasm-cli toywasm-lib m)
set_target_properties(toywasm-cli PROPERTIES OUTPUT_NAME toywasm)

add_test(NAME toywasm-cli-simple-module COMMAND
	toywasm-cli --load spectest.wasm --invoke "print_i32 123"
)
set_tests_properties(toywasm-cli-simple-module PROPERTIES ENVIRONMENT "${TEST_ENV}")

# some tests are not compatible with multi-memory
# (binary.wast, imports.wast, memory.wast)
# REVISIT: disable only those tests.
if(NOT TOYWASM_ENABLE_WASM_MULTI_MEMORY)
add_test(NAME toywasm-cli-wasm3-spec-test
	# Note: arbitrary limits for stack overflow tests in call.wast.
	# (--max-frames and --max-stack-cells)
	COMMAND ./test/run-wasm3-spec-test-opam-2.0.0.sh --exec "${CMAKE_BINARY_DIR}/toywasm --max-frames 2000 --max-stack-cells 10000 --repl --repl-prompt wasm3" --timeout 60 --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasm3-spec-test PROPERTIES LABELS "spec")
endif()

if(TOYWASM_ENABLE_WASM_EXTENDED_CONST)
add_test(NAME toywasm-cli-wasm3-spec-test-extended-const
	COMMAND ./test/run-wasm3-spec-test-extended-const.sh --exec "${CMAKE_BINARY_DIR}/toywasm --repl --repl-prompt wasm3" --timeout 60 --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test-extended-const PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasm3-spec-test-extended-const PROPERTIES LABELS "spec")
endif()

if(TOYWASM_ENABLE_WASM_MULTI_MEMORY)
add_test(NAME toywasm-cli-wasm3-spec-test-multi-memory
	COMMAND ./test/run-wasm3-spec-test-multi-memory.sh --exec "${CMAKE_BINARY_DIR}/toywasm --repl --repl-prompt wasm3" --timeout 60 --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test-multi-memory PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasm3-spec-test-multi-memory PROPERTIES LABELS "spec")
endif()

if(TOYWASM_ENABLE_WASM_TAILCALL)
add_test(NAME toywasm-cli-wasm3-spec-test-tailcall
	COMMAND ./test/run-wasm3-spec-test-tailcall.sh --exec "${CMAKE_BINARY_DIR}/toywasm --repl --repl-prompt wasm3" --timeout 60 --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test-tailcall PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasm3-spec-test-tailcall PROPERTIES LABELS "spec")
endif()

if(TOYWASM_ENABLE_WASM_THREADS)
add_test(NAME toywasm-cli-wasm3-spec-test-threads
	COMMAND ./test/run-wasm3-spec-test-threads.sh --exec "${CMAKE_BINARY_DIR}/toywasm --repl --repl-prompt wasm3" --timeout 60 --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test-threads PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasm3-spec-test-threads PROPERTIES LABELS "spec")
endif()

add_test(NAME toywasm-cli-wasi-testsuite
	COMMAND ./test/run-wasi-testsuite.sh
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasi-testsuite PROPERTIES ENVIRONMENT "${TEST_ENV};TOYWASM=${CMAKE_BINARY_DIR}/toywasm")
set_tests_properties(toywasm-cli-wasi-testsuite PROPERTIES LABELS "wasi-testsuite")

add_test(NAME toywasm-cli-wasmtime-wasi-tests
	# Note: poll_oneoff_stdio behaves differently on the condition on stdin
	COMMAND true | ./test/run-wasmtime-wasi-tests.sh "${CMAKE_BINARY_DIR}/toywasm --wasi --wasi-dir ."
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasmtime-wasi-tests PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasmtime-wasi-tests PROPERTIES LABELS "wasmtime-wasi-tests")

add_test(NAME toywasm-cli-spidermonkey
	COMMAND ./test/run-spidermonkey.sh ${CMAKE_BINARY_DIR}/toywasm --wasi
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-spidermonkey PROPERTIES ENVIRONMENT "${TEST_ENV}")

add_test(NAME toywasm-cli-ffmpeg
	COMMAND ./test/run-ffmpeg.sh ${CMAKE_BINARY_DIR}/toywasm --wasi --wasi-dir .video --
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-ffmpeg PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-ffmpeg PROPERTIES LABELS "slow")

if(NOT TOYWASM_ENABLE_WASM_MULTI_MEMORY)
add_test(NAME toywasm-cli-wasm3-spec-test-disable-optimizations
	# Note: arbitrary limits for stack overflow tests in call.wast.
	# (--max-frames and --max-stack-cells)
	COMMAND ./test/run-wasm3-spec-test-opam-2.0.0.sh --exec "${CMAKE_BINARY_DIR}/toywasm --disable-jump-table --disable-resulttype-cellidx --disable-localtype-cellidx --max-frames 2000 --max-stack-cells 10000 --repl --repl-prompt wasm3" --timeout 60 --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test-disable-optimizations PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasm3-spec-test-disable-optimizations PROPERTIES LABELS "spec")
endif()

add_test(NAME toywasm-cli-wasm3-wasi-test
	COMMAND ./test/run-wasm3-wasi-test.sh --exec "${CMAKE_BINARY_DIR}/toywasm --wasi --wasi-dir ." --separate-args --timeout 1200
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-wasi-test PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasm3-wasi-test PROPERTIES LABELS "slow")

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
find_package(cmocka REQUIRED)

# compat with cmocka <1.1.6
# https://gitlab.com/cmocka/cmocka/-/commit/e4c51ce9a4792d7d301d49e4e962036bfaa7f75e
if(NOT TARGET cmocka::cmocka)
add_library(cmocka::cmocka STATIC IMPORTED)
set_target_properties(cmocka::cmocka
                      PROPERTIES
                      IMPORTED_LOCATION "${CMOCKA_LIBRARIES}"
                      INTERFACE_INCLUDE_DIRECTORIES "${CMOCKA_INCLUDE_DIR}")
endif()

set(test_sources
	"test/test.c"
)

add_executable(toywasm-test ${test_sources})
# Force native build because cmocka from homebrew is not universal binary.
set_target_properties(toywasm-test
    PROPERTIES OSX_ARCHITECTURES ${CMAKE_HOST_SYSTEM_PROCESSOR})
target_link_libraries(toywasm-test toywasm-lib m ${CMOCKA_LIBRARY})

add_test(NAME toywasm-test COMMAND toywasm-test)
set_tests_properties(toywasm-test PROPERTIES ENVIRONMENT "${TEST_ENV}")
target_link_libraries(toywasm-test cmocka::cmocka)
endif()

# XXX Is there a way to create the file list from install() commands?
# XXX Is there a way to express dependency on "install" target?
find_program(PAX pax)
add_custom_target(tar
	COMMAND ${PAX} -wzf toywasm-${TOYWASM_VERSION}${TOYWASM_TARBALL_SUFFIX}.tgz
        -s!${CMAKE_INSTALL_PREFIX}/!!
        ${CMAKE_INSTALL_PREFIX}/bin/toywasm
        ${CMAKE_INSTALL_PREFIX}/share/doc/toywasm/LICENSE
        ${CMAKE_INSTALL_PREFIX}/share/doc/toywasm/README.md
)

install(TARGETS toywasm-cli DESTINATION bin)
install(FILES LICENSE README.md DESTINATION share/doc/toywasm)
