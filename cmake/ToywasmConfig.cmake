include(CMakeDependentOption)

if(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# TOYWASM_USE_SEPARATE_EXECUTE=ON -> faster execution
# TOYWASM_USE_SEPARATE_EXECUTE=OFF -> smaller code
option(TOYWASM_USE_SEPARATE_EXECUTE "Use separate execute callback" ON)
option(TOYWASM_USE_SEPARATE_VALIDATE "Use separate validation callback" OFF)

# TOYWASM_USE_TAILCALL=ON
#   enable -mtail-call for wasm target
#
# TOYWASM_USE_SEPARATE_EXECUTE=ON
# TOYWASM_USE_TAILCALL=ON
# TOYWASM_FORCE_USE_TAILCALL=OFF
#   if musttail attribute is available, use it and rely on the tail call
#   opitimzation. it usually produces faster code.
#
# TOYWASM_USE_SEPARATE_EXECUTE=ON
# TOYWASM_USE_TAILCALL=ON
# TOYWASM_FORCE_USE_TAILCALL=ON
#   rely on the tail call optimization even if musttail is not available.
#   you need to investigate the generated binary to see if it was safe or not.
option(TOYWASM_USE_TAILCALL "Rely on tail call optimization if musttail is available" ON)
cmake_dependent_option(TOYWASM_FORCE_USE_TAILCALL
    "Assume tail call optimization forcibly"
    OFF
    "TOYWASM_USE_TAILCALL"
    OFF)

# TOYWASM_USE_SIMD=ON -> use -msimd128 for wasm target
option(TOYWASM_USE_SIMD "Use SIMD" OFF)

# toywasm uses a few enums like "enum valtype", for which 1 byte is enough.
option(TOYWASM_USE_SHORT_ENUMS "Use -fshort-enum" ON)

# TOYWASM_USE_USER_SCHED=ON uses a simple userland scheduler instead of pthread.
option(TOYWASM_USE_USER_SCHED "Use userland scheduler" OFF)

# options to enable/disable "toywasm --trace" stuff
option(TOYWASM_ENABLE_TRACING "Enable xlog_trace" OFF)
cmake_dependent_option(TOYWASM_ENABLE_TRACING_INSN
    "Enable per-instruction xlog_trace"
    OFF
    "TOYWASM_ENABLE_TRACING"
    OFF)

# Sort module exports to speed up the uniqueness check.
# O(n^2) -> O(n*log(n))
option(TOYWASM_SORT_EXPORTS "Sort module export" ON)

# TOYWASM_USE_JUMP_BINARY_SEARCH=ON makes the jump table binary search.
# otherwise, linear search is used.
option(TOYWASM_USE_JUMP_BINARY_SEARCH "Enable binary search for jump tables" ON)

# TOYWASM_USE_JUMP_CACHE and TOYWASM_JUMP_CACHE2_SIZE controls
# two independent jump table caching logic.
# there is little reasons to enable both of them.
# (the latter can be disabled with TOYWASM_JUMP_CACHE2_SIZE=0.)
option(TOYWASM_USE_JUMP_CACHE "Enable single-entry cache for jump tables" OFF)
set(TOYWASM_JUMP_CACHE2_SIZE "4" CACHE STRING "The size of jump cache")

# TOYWASM_USE_LOCALS_CACHE=ON -> faster execution
# TOYWASM_USE_LOCALS_CACHE=OFF -> slightly smaller code and exec_context
option(TOYWASM_USE_LOCALS_CACHE "Enable current_locals" ON)

# use separate stack for operand stack and function locals or not
option(TOYWASM_USE_SEPARATE_LOCALS "Separate locals and stack" ON)

# control how to store values for wasm operand stack, locals, and tables.
#
# TOYWASM_USE_SMALL_CELLS=ON
#    i32,f32 -> occupies 32 bit memory
#    i64,f64 -> occupies 64 bit memory
#    v128    -> occupies 128 bit memory
#
# TOYWASM_USE_SMALL_CELLS=OFF
# TOYWASM_ENABLE_WASM_SIMD=OFF
#    any value occupies 64 bit memory
#
# TOYWASM_USE_SMALL_CELLS=OFF
# TOYWASM_ENABLE_WASM_SIMD=ON
#    any value occupies 128 bit memory
#
# TOYWASM_USE_SMALL_CELLS=OFF produces simpler and in many cases faster code.
option(TOYWASM_USE_SMALL_CELLS "Use smaller stack cells" ON)

# enable indexes for faster lookup for resulttype and localtype respectively.
cmake_dependent_option(TOYWASM_USE_RESULTTYPE_CELLIDX
    "Index local lookup for resulttype"
    ON
    "TOYWASM_USE_SMALL_CELLS"
    OFF)
cmake_dependent_option(TOYWASM_USE_LOCALTYPE_CELLIDX
    "Index local lookup for localtype"
    ON
    "TOYWASM_USE_SMALL_CELLS"
    OFF)

# TOYWASM_USE_LOCALS_FAST_PATH=ON -> faster execution
# TOYWASM_USE_LOCALS_FAST_PATH=OFF -> slightly smaller code and exec_context
cmake_dependent_option(TOYWASM_USE_LOCALS_FAST_PATH
    "Enable fast path of frame_locals_cellidx"
    ON
    "TOYWASM_USE_RESULTTYPE_CELLIDX;TOYWASM_USE_LOCALTYPE_CELLIDX"
    OFF)

# TOYWASM_PREALLOC_SHARED_MEMORY=ON
#   allocate the max possible size of shared memories on instantiation.
#   simpler but can waste a lot of memory.
# TOYWASM_PREALLOC_SHARED_MEMORY=OFF
#   on-demand (on memory.grow) allocation of shared memories.
#   can save memory, but slower and very complex memory.grow processing.
cmake_dependent_option(TOYWASM_PREALLOC_SHARED_MEMORY
    "Preallocate shared memory"
    OFF
    "TOYWASM_ENABLE_WASM_THREADS"
    OFF)

# enable logic to write a module to a file.
# currently it's only used by repl ":save" command.
option(TOYWASM_ENABLE_WRITER "Enable module writer" ON)

# enable SIMD. we made this an option because it's large.
option(TOYWASM_ENABLE_WASM_SIMD "Enable SIMD" ON)

# enable each wasm proposals.
option(TOYWASM_ENABLE_WASM_EXCEPTION_HANDLING "Enable exception-handling proposal" OFF)
set(TOYWASM_EXCEPTION_MAX_CELLS "4" CACHE STRING "The max size of exception")
option(TOYWASM_ENABLE_WASM_EXTENDED_CONST "Enable extended-const proposal" OFF)
option(TOYWASM_ENABLE_WASM_MULTI_MEMORY "Enable multi-memory proposal" OFF)
option(TOYWASM_ENABLE_WASM_TAILCALL "Enable WASM tail-call proposal" OFF)
option(TOYWASM_ENABLE_WASM_THREADS "Enable WASM threads proposal" OFF)
option(TOYWASM_ENABLE_WASM_CUSTOM_PAGE_SIZES "Enable WASM custom-page-sizes proposal" OFF)
option(TOYWASM_ENABLE_WASM_NAME_SECTION "Enable name section" ON)

# enable WASI.
option(TOYWASM_ENABLE_WASI "Enable WASI snapshow preview1" ON)

cmake_dependent_option(TOYWASM_ENABLE_WASI_LITTLEFS
    "Enable WASI littlefs support"
    OFF
    "TOYWASM_ENABLE_WASI"
    OFF)

cmake_dependent_option(TOYWASM_ENABLE_LITTLEFS_STATS
    "Enable littlefs stats (for debug)"
    OFF
    "TOYWASM_ENABLE_WASI"
    OFF)

# enable wasi-threads.
cmake_dependent_option(TOYWASM_ENABLE_WASI_THREADS
    "Enable wasi-threads proposal"
    OFF
    "TOYWASM_ENABLE_WASM_THREADS"
    OFF)

# experimental emscripten-style shared library
# https://github.com/WebAssembly/tool-conventions/blob/main/DynamicLinking.md
option(TOYWASM_ENABLE_DYLD "Enable shared library support" OFF)
cmake_dependent_option(TOYWASM_ENABLE_DYLD_DLFCN
    "Enable dlopen-like host API"
    OFF
    "TOYWASM_ENABLE_DYLD"
    OFF)

option(TOYWASM_ENABLE_FUZZER "Enable fuzzer" OFF)

option(TOYWASM_BUILD_UNITTEST "Build toywasm-test" ON)

if(TOYWASM_ENABLE_FUZZER)
add_compile_options(-fsanitize=fuzzer-no-link)
add_compile_definitions(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
endif()

if(NOT DEFINED USE_LSAN)
if(CMAKE_BUILD_TYPE MATCHES "Debug")
set(USE_LSAN ON)
else()
set(USE_LSAN OFF)
endif()
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
#
# lld doesn't seem to support riscv relaxizations
# ld.lld: error: /usr/bin/../lib/gcc-cross/riscv64-linux-gnu/9/../../../../riscv64-linux-gnu/lib/crt1.o:(.text+0x0): relocation R_RISCV_ALIGN requires unimplemented linker relaxation; recompile with -mno-relax
if(NOT TRIPLET MATCHES "s390" AND NOT TRIPLET MATCHES "riscv64")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
else()
set(USE_IPO OFF)
endif()
endif()
endif()

# clang-13 on ubuntu/focal:
# ld.lld: error: lto.tmp: cannot link object files with different floating-point ABI
#if(TRIPLET MATCHES "riscv64")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-plugin-opt=-target-abi=lp64d")
#endif()

if(TRIPLET MATCHES "i386")
# x87 doesn't preserve sNaN as IEEE 754 and wasm expect.
# unfortunately, clang doesn't have -mno-fp-ret-in-387.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse2 -mfpmath=sse")
endif()

if(CMAKE_C_COMPILER_TARGET MATCHES "wasm")
if(TOYWASM_USE_TAILCALL)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mtail-call")
endif()
if(TOYWASM_USE_SIMD)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msimd128")
endif()
endif()

if(NOT DEFINED USE_TSAN)
# Off by default because it's incompatible with ASAN and LSAN
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
# Note about WASM_MAX_MEMORY:
# * the spec requires shared memory have an explicit max-memory limit.
# * 65536 pages specified below is the largest value allowed by the
#   spec. Because it's difficult/impossible to make a build-time estimation
#   of memory consumption for an intepreter, we simply specify the largest
#   possible value.
# * While toywasm with TOYWASM_PREALLOC_SHARED_MEMORY=OFF can handle
#   dynamic on-demand (on memory.grow) allocation of shared memory,
#   some runtimes (eg. WAMR as of writing this) simply commits the max size
#   of shared memory on module instantiation to simplify the implementation.
#   If you intend to run this module on such runtimes, it's probably safer
#   to use a smaller WASM_MAX_MEMORY.
if(NOT DEFINED WASM_MAX_MEMORY)
set(WASM_MAX_MEMORY 4294967296)  # 65536 pages
endif()
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--max-memory=${WASM_MAX_MEMORY}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--import-memory")
# require LLVM >=16
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--export-memory")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
if(USE_TSAN)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=thread")
endif()
endif()
endif()

# GCC doesn't seem to have a way to only allow statement expressions
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic -Wno-gnu-statement-expression")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wvla -Werror")
if(CMAKE_C_COMPILER_ID STREQUAL GNU)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-maybe-uninitialized")
endif()

# GCC 9's builtin ceil() etc seems to propagate sNaN as it is.
# GCC 11 seems ok.
if(CMAKE_C_COMPILER_ID STREQUAL GNU)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-builtin")
endif()

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fomit-frame-pointer")
#set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Xclang -fmerge-functions")
#set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -mllvm -mergefunc-use-aliases")

if (NOT CMAKE_BUILD_TYPE MATCHES "Debug")
# Note: Release build disables assertions and thus yields a lot of
# used variables. We are not interested in fixing them.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-warning-option -Wno-unused-but-set-variable -Wno-unused-variable -Wno-return-type")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
# https://clang.llvm.org/docs/ThreadSafetyAnalysis.html
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wthread-safety")
endif()

if(NOT DEFINED USE_UBSAN)
if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND CMAKE_BUILD_TYPE MATCHES "Debug")
set(USE_UBSAN ON)
else()
set(USE_UBSAN OFF)
endif()
endif()
if(USE_UBSAN)
list(APPEND SANITIZER_FLAGS "-fsanitize=alignment")
list(APPEND SANITIZER_FLAGS "-fno-sanitize-recover=alignment")
list(APPEND SANITIZER_FLAGS "-fsanitize=undefined")
list(APPEND SANITIZER_FLAGS "-fno-sanitize-recover=undefined")
list(APPEND SANITIZER_FLAGS "-fsanitize=integer")
list(APPEND SANITIZER_FLAGS "-fno-sanitize-recover=integer")
# unsigned-shift-base was introduced by LLVM 12.
if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 12.0.0)
# unsigned-shift-base: not an undefined behavior. used in leb128 etc.
list(APPEND SANITIZER_FLAGS "-fno-sanitize=unsigned-shift-base")
endif()
# unsigned-integer-overflow: not an undefined behavior. used in leb128 etc.
list(APPEND SANITIZER_FLAGS "-fno-sanitize=unsigned-integer-overflow")
# implicit-integer-sign-change: we relies on it for a lot of opcodes.
list(APPEND SANITIZER_FLAGS "-fno-sanitize=implicit-integer-sign-change")
# we use NULL+0 in some places. often with VEC_NEXELEM.
list(APPEND SANITIZER_FLAGS "-fno-sanitize=pointer-overflow")
endif()

if(NOT DEFINED USE_ASAN)
if(CMAKE_BUILD_TYPE MATCHES "Debug")
set(USE_ASAN ON)
else()
set(USE_ASAN OFF)
endif()
endif()
if(USE_ASAN)
list(APPEND SANITIZER_FLAGS "-fsanitize=address")
endif()

if(USE_LSAN)
list(APPEND SANITIZER_FLAGS "-fsanitize=leak")
set(ASAN_DETECT_LEAKS 1)
else()
set(ASAN_DETECT_LEAKS 0)
endif()

list(JOIN SANITIZER_FLAGS " " SANITIZER_FLAGS_STR)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS_STR}")

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
message(STATUS "CMAKE_C_COMPILER_VERSION: ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "CMAKE_AR: ${CMAKE_AR}")
message(STATUS "CMAKE_C_COMPILER_AR: ${CMAKE_C_COMPILER_AR}")
message(STATUS "CMAKE_RANLIB: ${CMAKE_RANLIB}")
message(STATUS "CMAKE_C_COMPILER_RANLIB: ${CMAKE_C_COMPILER_RANLIB}")
message(STATUS "BUILD_TESTING: ${BUILD_TESTING}")
message(STATUS "USE_IPO: ${USE_IPO}")
message(STATUS "USE_ASAN: ${USE_ASAN}")
message(STATUS "USE_LSAN: ${USE_LSAN}")
message(STATUS "USE_TSAN: ${USE_TSAN}")
message(STATUS "USE_UBSAN: ${USE_UBSAN}")

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
