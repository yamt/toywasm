if(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "default" FORCE)
endif()

option(TOYWASM_USE_SEPARATE_EXECUTE "Use separate execute callback" ON)
option(TOYWASM_USE_TAILCALL "Use tailcall" ON)
option(TOYWASM_USE_SHORT_ENUMS "Use -fshort-enum" ON)
option(TOYWASM_USE_USER_SCHED "Use userland scheduler" OFF)
option(TOYWASM_ENABLE_TRACING "Enable xlog_trace" OFF)
option(TOYWASM_ENABLE_TRACING_INSN "Enable per-instruction xlog_trace" OFF)
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
option(TOYWASM_PREALLOC_SHARED_MEMORY "Preallocate shared memory" OFF)
option(TOYWASM_ENABLE_WRITER "Enable module writer" ON)
option(TOYWASM_ENABLE_WASM_EXTENDED_CONST "Enable extended-const proposal" OFF)
option(TOYWASM_ENABLE_WASM_MULTI_MEMORY "Enable multi-memory proposal" OFF)
option(TOYWASM_ENABLE_WASM_TAILCALL "Enable WASM tail-call proposal" OFF)
option(TOYWASM_ENABLE_WASM_THREADS "Enable WASM threads proposal" OFF)
option(TOYWASM_ENABLE_WASI "Enable WASI snapshow preview1" ON)
option(TOYWASM_ENABLE_WASI_THREADS "Enable wasi-threads proposal" OFF)

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

# clang-14: error: unsupported option '-fsanitize=leak' for target 'wasm32-unknown-wasi'
# clang-14: error: unsupported option '-fsanitize=address' for target 'wasm32-unknown-wasi'
if(CMAKE_C_COMPILER_TARGET MATCHES "wasm")
set(USE_LSAN OFF)
set(USE_ASAN OFF)
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

if(TOYWASM_USE_TAILCALL)
if(CMAKE_C_COMPILER_TARGET MATCHES "wasm")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mtail-call")
endif()
endif()

if(NOT DEFINED USE_TSAN)
set(USE_TSAN OFF)
endif()
# TOYWASM_ENABLE_WASM_THREADS might require pthread
if(NOT TOYWASM_ENABLE_WASM_THREADS)
set(TOYWASM_USE_USER_SCHED OFF)
endif()
if(TOYWASM_ENABLE_WASM_THREADS AND NOT TOYWASM_USE_USER_SCHED)
# https://cmake.org/cmake/help/latest/module/FindThreads.html
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
if (NOT THREADS_FOUND)
message(WARNING "Enabling TOYWASM_USE_USER_SCHED because pthread was not found")
set(TOYWASM_USE_USER_SCHED ON)
else()
if(CMAKE_C_COMPILER_TARGET MATCHES "wasm")
# https://reviews.llvm.org/D130053
# https://llvm.org/docs/LangRef.html#thread-local-storage-models
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ftls-model=local-exec")
set(WASM_MAX_MEMORY 67108864)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--max-memory=${WASM_MAX_MEMORY}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--import-memory")
# require LLVM >=16
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--export-memory")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
if(USE_TSAN)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=thread")
endif()
endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")
if(CMAKE_C_COMPILER_ID STREQUAL GNU)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")
endif()

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fomit-frame-pointer")

if (NOT CMAKE_BUILD_TYPE MATCHES "Debug")
# Note: Release build disables assertions and thus yields a lot of
# used variables. We are not interested in fixing them.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-warning-option -Wno-unused-but-set-variable -Wno-unused-variable -Wno-return-type")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
# https://clang.llvm.org/docs/ThreadSafetyAnalysis.html
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wthread-safety")
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
message(STATUS "USE_TSAN: ${USE_TSAN}")

find_package(Git REQUIRED)
execute_process(
	COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --always --match "v*"
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	OUTPUT_VARIABLE GIT_DESCRIBE_OUTPUT
	RESULT_VARIABLE GIT_DESCRIBE_RESULT
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
# RESULT_VARIABLE can be an error string or exit status.
# see https://cmake.org/cmake/help/latest/command/execute_process.html
if((NOT GIT_DESCRIBE_RESULT) OR GIT_DESCRIBE_RESULT EQUAL 0)
set(TOYWASM_VERSION ${GIT_DESCRIBE_OUTPUT})
endif()
if(DEFINED TOYWASM_VERSION)
message(STATUS "toywasm version: ${TOYWASM_VERSION}")
else()
message(WARNING "failed to determine version. using \"unknown\".")
set(TOYWASM_VERSION "unknown")
endif()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/lib/toywasm_version.h.in"
	"${CMAKE_BINARY_DIR}/toywasm_version.h")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/lib/toywasm_config.h.in"
	"${CMAKE_BINARY_DIR}/toywasm_config.h")
include_directories(${CMAKE_BINARY_DIR})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/lib/toywasm_config.c.in"
	"${CMAKE_BINARY_DIR}/toywasm_config.c")
