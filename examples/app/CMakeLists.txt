cmake_minimum_required(VERSION 3.16)

project(app LANGUAGES C)

find_package(toywasm REQUIRED)

set(app_sources
	"app.c"
)

add_executable(app ${app_sources})
target_link_libraries(app toywasm-lib m)

# XXX the library is built with short enum.
# maybe we can use INTERFACE_COMPILE_OPTIONS.
# but i feel a bit uncomfortable with it.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fshort-enums")

# for a library with IPO enabled
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
if (CMAKE_C_COMPILER_ID MATCHES "Clang")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
endif()
endif()
