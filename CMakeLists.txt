cmake_minimum_required(VERSION 3.16)

include(cmake/HomebrewLLVM.cmake)

project(toywasm LANGUAGES C)

# We use C11 features like _Atomic, _Static_assert
set(CMAKE_C_STANDARD 11)
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

add_subdirectory(lib)
if(TOYWASM_ENABLE_WASI)
add_subdirectory(libwasi)
endif()
if(TOYWASM_ENABLE_WASI_LITTLEFS)
add_subdirectory(libwasi_littlefs)
endif()
if(TOYWASM_ENABLE_DYLD)
add_subdirectory(libdyld)
endif()

if(CMAKE_C_COMPILER_TARGET MATCHES "wasm")
set(TOYWASM_CLI "${CMAKE_CURRENT_SOURCE_DIR}/test/toywasm-on-toywasm.py")
if(NOT DEFINED ENV{TOYWASM_NATIVE})
list(APPEND TEST_ENV "TOYWASM_NATIVE=${CMAKE_CURRENT_SOURCE_DIR}/build.native/toywasm")
endif()
if(NOT DEFINED ENV{TOYWASM_WASM})
list(APPEND TEST_ENV "TOYWASM_WASM=${CMAKE_BINARY_DIR}/toywasm")
endif()
else()
set(TOYWASM_CLI "${CMAKE_BINARY_DIR}/toywasm")
endif()

# for wasi-testsuite-adapter.py
list(APPEND TEST_ENV "TEST_RUNTIME_EXE=${TOYWASM_CLI}")

# cli

set(cli_sources
	"cli/main.c"
	"cli/repl.c"
)

add_executable(toywasm-cli ${cli_sources})
target_link_libraries(toywasm-cli toywasm-lib-core
	$<$<BOOL:${TOYWASM_ENABLE_WASI}>:toywasm-lib-wasi>
	$<$<BOOL:${TOYWASM_ENABLE_WASI_LITTLEFS}>:toywasm-lib-wasi-littlefs>
	$<$<BOOL:${TOYWASM_ENABLE_DYLD}>:toywasm-lib-dyld>
    m)
set_target_properties(toywasm-cli PROPERTIES OUTPUT_NAME toywasm)

add_test(NAME toywasm-cli-simple-module COMMAND
	${TOYWASM_CLI} --load=spectest.wasm "--invoke=print_i32 123"
)
set_tests_properties(toywasm-cli-simple-module PROPERTIES ENVIRONMENT "${TEST_ENV}")

add_test(NAME toywasm-cli-timeout COMMAND
	${TOYWASM_CLI} --timeout=100 infiniteloop.wasm
)
set_tests_properties(toywasm-cli-timeout PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-timeout PROPERTIES LABELS "timeout")
set_tests_properties(toywasm-cli-timeout PROPERTIES WILL_FAIL ON)

add_test(NAME toywasm-cli-start-timeout COMMAND
	${TOYWASM_CLI} --timeout=100 infiniteloop_in_start.wasm
)
set_tests_properties(toywasm-cli-start-timeout PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-start-timeout PROPERTIES LABELS "timeout")
set_tests_properties(toywasm-cli-start-timeout PROPERTIES WILL_FAIL ON)

if(TOYWASM_ENABLE_WASI_THREADS)
add_test(NAME toywasm-cli-timeout-wasi-threads COMMAND
	${TOYWASM_CLI} --wasi --timeout=100 infiniteloops.wasm
)
set_tests_properties(toywasm-cli-timeout-wasi-threads PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-timeout-wasi-threads PROPERTIES LABELS "timeout")
set_tests_properties(toywasm-cli-timeout-wasi-threads PROPERTIES WILL_FAIL ON)
endif()

# some tests are not compatible with multi-memory
# (binary.wast, imports.wast, memory.wast)
# REVISIT: disable only those tests.
if(NOT TOYWASM_ENABLE_WASM_MULTI_MEMORY)
add_test(NAME toywasm-cli-wasm3-spec-test
	# Note: arbitrary limits for stack overflow tests in call.wast.
	# (--max-frames and --max-stack-cells)
	COMMAND ./test/run-wasm3-spec-test-opam-2.0.0.sh --exec "${TOYWASM_CLI} --max-frames=201 --max-stack-cells=1000 --repl --repl-prompt=wasm3" --timeout 60 --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasm3-spec-test PROPERTIES LABELS "spec")
endif()

if(TOYWASM_ENABLE_WASM_SIMD)
add_test(NAME toywasm-cli-wasm3-spec-test-simd
	COMMAND ./test/run-wasm3-spec-test-simd.sh --exec "${TOYWASM_CLI} --repl --repl-prompt=wasm3" --timeout 60 --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test-simd PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasm3-spec-test-simd PROPERTIES LABELS "spec;simd")
endif()

if(TOYWASM_ENABLE_WASM_EXTENDED_CONST)
add_test(NAME toywasm-cli-wasm3-spec-test-extended-const
	COMMAND ./test/run-wasm3-spec-test-extended-const.sh --exec "${TOYWASM_CLI} --repl --repl-prompt=wasm3" --timeout 60 --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test-extended-const PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasm3-spec-test-extended-const PROPERTIES LABELS "spec")
endif()

if(TOYWASM_ENABLE_WASM_MULTI_MEMORY)
add_test(NAME toywasm-cli-wasm3-spec-test-multi-memory
	COMMAND ./test/run-wasm3-spec-test-multi-memory.sh --exec "${TOYWASM_CLI} --repl --repl-prompt=wasm3" --timeout 60 --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test-multi-memory PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasm3-spec-test-multi-memory PROPERTIES LABELS "spec")
endif()

if(TOYWASM_ENABLE_WASM_TAILCALL)
add_test(NAME toywasm-cli-wasm3-spec-test-tailcall
	COMMAND ./test/run-wasm3-spec-test-tailcall.sh --exec "${TOYWASM_CLI} --repl --repl-prompt=wasm3" --timeout 60 --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test-tailcall PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasm3-spec-test-tailcall PROPERTIES LABELS "spec")
endif()

if(TOYWASM_ENABLE_WASM_THREADS)
add_test(NAME toywasm-cli-wasm3-spec-test-threads
	COMMAND ./test/run-wasm3-spec-test-threads.sh --exec "${TOYWASM_CLI} --repl --repl-prompt=wasm3" --timeout 60 --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test-threads PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasm3-spec-test-threads PROPERTIES LABELS "spec:wasm-threads")
endif()

if(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING)
add_test(NAME toywasm-cli-exception-handling-test
	COMMAND ./test.sh
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wat/eh
)
set_tests_properties(toywasm-cli-exception-handling-test PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-exception-handling-test PROPERTIES LABELS "exception-handling")

add_test(NAME toywasm-cli-exception-handling-test-disable-jump-table
	COMMAND ./test.sh
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wat/eh
)
set_tests_properties(toywasm-cli-exception-handling-test-disable-jump-table PROPERTIES ENVIRONMENT "${TEST_ENV};TEST_RUNTIME_EXE=${TOYWASM_CLI} --disable-jump-table")
set_tests_properties(toywasm-cli-exception-handling-test-disable-jump-table PROPERTIES LABELS "exception-handling")
endif()

if(TOYWASM_ENABLE_WASI)
add_test(NAME toywasm-cli-wasi-testsuite
	COMMAND ./test/run-wasi-testsuite.sh
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasi-testsuite PROPERTIES ENVIRONMENT "${TEST_ENV};TOYWASM=${TOYWASM_CLI};$<$<BOOL:${TOYWASM_ENABLE_WASI_THREADS}>:TESTS=proposals/wasi-threads/>")
set_tests_properties(toywasm-cli-wasi-testsuite PROPERTIES LABELS "wasi-testsuite")

if(TOYWASM_ENABLE_WASI_LITTLEFS)
add_test(NAME toywasm-cli-wasi-testsuite-littlefs
	COMMAND ./test/run-wasi-testsuite.sh
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasi-testsuite-littlefs PROPERTIES ENVIRONMENT "${TEST_ENV};TOYWASM=${TOYWASM_CLI};$<$<BOOL:${TOYWASM_ENABLE_WASI_THREADS}>:TESTS=proposals/wasi-threads/>;TOYWASM_WASI_FILESYSTEM_TYPE=littlefs")
set_tests_properties(toywasm-cli-wasi-testsuite-littlefs PROPERTIES LABELS "wasi-testsuite;littlefs")
endif()

add_test(NAME toywasm-cli-wasmtime-wasi-tests
	COMMAND ./test/run-wasmtime-wasi-tests.sh "${TOYWASM_CLI} --wasi --wasi-dir=."
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasmtime-wasi-tests PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasmtime-wasi-tests PROPERTIES LABELS "wasmtime-wasi-tests")

add_test(NAME toywasm-cli-js-wasm
	COMMAND ./test/js-wasm.sh test/pi2.js
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-js-wasm PROPERTIES ENVIRONMENT "${TEST_ENV};TEST_RUNTIME_EXE=${TOYWASM_CLI} --wasi --wasi-dir=test --")
set_tests_properties(toywasm-cli-js-wasm PROPERTIES LABELS "app")

add_test(NAME toywasm-cli-spidermonkey
	COMMAND ./test/run-spidermonkey.sh ${TOYWASM_CLI} --wasi
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-spidermonkey PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-spidermonkey PROPERTIES LABELS "app")

add_test(NAME toywasm-cli-ffmpeg
	COMMAND ./test/run-ffmpeg.sh ${TOYWASM_CLI} --wasi --wasi-dir=.video --
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-ffmpeg PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-ffmpeg PROPERTIES LABELS "slow;app")

add_test(NAME toywasm-cli-wcpl
	COMMAND ./test/run-wcpl.sh
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wcpl PROPERTIES ENVIRONMENT "${TEST_ENV};TEST_RUNTIME_EXE=${TOYWASM_CLI} --wasi --wasi-dir=. --")
set_tests_properties(toywasm-cli-wcpl PROPERTIES LABELS "slow;app")
endif() # TOYWASM_ENABLE_WASI

if(NOT TOYWASM_ENABLE_WASM_MULTI_MEMORY)
add_test(NAME toywasm-cli-wasm3-spec-test-disable-optimizations
	# Note: arbitrary limits for stack overflow tests in call.wast.
	# (--max-frames and --max-stack-cells)
	COMMAND ./test/run-wasm3-spec-test-opam-2.0.0.sh --exec "${TOYWASM_CLI} --disable-jump-table --disable-resulttype-cellidx --disable-localtype-cellidx --max-frames=201 --max-stack-cells=1000 --repl --repl-prompt=wasm3" --timeout 60 --spectest ${CMAKE_BINARY_DIR}/spectest.wasm
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-spec-test-disable-optimizations PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasm3-spec-test-disable-optimizations PROPERTIES LABELS "spec")
endif()

if(TOYWASM_ENABLE_WASI)
add_test(NAME toywasm-cli-wasm3-wasi-test
	COMMAND ./test/run-wasm3-wasi-test.sh --exec "${TOYWASM_CLI} --wasi --wasi-dir=." --separate-args --timeout 1200
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(toywasm-cli-wasm3-wasi-test PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-wasm3-wasi-test PROPERTIES LABELS "slow")
endif() # TOYWASM_ENABLE_WASI

if(TOYWASM_ENABLE_WASI_LITTLEFS)
add_test(NAME toywasm-cli-littlefs-test
	COMMAND ./test.sh
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test/littlefs
)
set_tests_properties(toywasm-cli-littlefs-test PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-littlefs-test PROPERTIES LABELS "wasi;littlefs")
endif()

if(TOYWASM_ENABLE_DYLD_DLFCN)
add_test(NAME toywasm-cli-dyld-test
	COMMAND ./fetch-and-run-test.sh
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test/dyld
)
set_tests_properties(toywasm-cli-dyld-test PROPERTIES ENVIRONMENT "${TEST_ENV}")
set_tests_properties(toywasm-cli-dyld-test PROPERTIES LABELS "wasi;dyld")
endif()

# sample wasm files

if(BUILD_TESTING)
set(wat_files
	test/spectest.wat
	wat/infiniteloop.wat
	wat/infiniteloop_in_start.wat
	wat/wasi-threads/infiniteloops.wat
)

find_program(WAT2WASM wat2wasm REQUIRED)
foreach(wat ${wat_files})
	get_filename_component(f ${wat} NAME_WLE)
	set(wasm "${f}.wasm")
	add_custom_command(OUTPUT ${wasm}
		COMMAND ${WAT2WASM} --enable-all -o ${wasm} ${CMAKE_CURRENT_SOURCE_DIR}/${wat}
		MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${wat}
		COMMENT "Building ${wasm}")
	add_custom_target(build-${wasm} ALL DEPENDS ${wasm})
endforeach()
endif()

# unit test

if(BUILD_TESTING)
if(TOYWASM_BUILD_UNITTEST)
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
target_link_libraries(toywasm-test toywasm-lib-core m ${CMOCKA_LIBRARY})

add_test(NAME toywasm-test COMMAND toywasm-test)
set_tests_properties(toywasm-test PROPERTIES ENVIRONMENT "${TEST_ENV}")
target_link_libraries(toywasm-test cmocka::cmocka)
endif() # TOYWASM_BUILD_UNITTEST
endif()

# XXX Is there a way to create the file list from install() commands?
# for now, we assume we only have toywasm-installed files in
# CMAKE_INSTALL_PREFIX.
# XXX Is there a way to express dependency on "install" target?
find_program(PAX pax)
add_custom_target(tar
	COMMAND ${PAX} -wzf toywasm-${TOYWASM_VERSION}${TOYWASM_TARBALL_SUFFIX}.tgz
        -s!${CMAKE_INSTALL_PREFIX}/!!
        ${CMAKE_INSTALL_PREFIX}/*
)

install(TARGETS toywasm-cli DESTINATION bin)
install(FILES LICENSE README.md DESTINATION share/doc/toywasm)
install(FILES doc/annotations.md doc/check_interrupt.md DESTINATION share/doc/toywasm/doc)
