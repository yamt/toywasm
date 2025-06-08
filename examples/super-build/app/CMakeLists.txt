cmake_minimum_required(VERSION 3.16)

project(app LANGUAGES C)

find_package(toywasm-lib-core REQUIRED)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wvla -Werror")

set(app_sources
	"../../app/app.c"
)

add_executable(app ${app_sources})
target_link_libraries(app toywasm-lib-core m)
