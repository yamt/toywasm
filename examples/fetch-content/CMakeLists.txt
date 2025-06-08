# An example to fetch toywasm release assets and link to it
#
# Note: as toywasm release assets are built with thin LTO,
# this only works with a compatible version of toolchain.

cmake_minimum_required(VERSION 3.16)

project(app LANGUAGES C)

if(NOT DEFINED TOYWASM_URL)
    if(NOT DEFINED TOYWASM_TARGET)
        # make a guess
        if(CMAKE_C_COMPILER_TARGET MATCHES "wasm")
            set(TOYWASM_TARGET wasm32-wasi)
        elseif(APPLE)
            set(TOYWASM_TARGET macos-13.0)
        else()
            set(TOYWASM_TARGET ubuntu-22.04-amd64)
        endif()
    endif()
    set(TOYWASM_URL https://github.com/yamt/toywasm/releases/download/v68.0.0/toywasm-v68.0.0-${TOYWASM_TARGET}.tgz)
message(STATUS "TOYWASM_URL: ${TOYWASM_URL}")
endif()

include(FetchContent)
FetchContent_Declare(
    toywasm
    # Note: DOWNLOAD_EXTRACT_TIMESTAMP was added by cmake 3.23.
    # it seems older version doesn't complain. (i tested 3.22.1)
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    URL "${TOYWASM_URL}"
)
FetchContent_MakeAvailable(toywasm)
list(APPEND CMAKE_PREFIX_PATH ${toywasm_SOURCE_DIR})
find_package(toywasm-lib-core REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wvla -Werror")

set(app_sources
	"../app/app.c"
)

add_executable(app ${app_sources})
target_link_libraries(app toywasm-lib-core m)
