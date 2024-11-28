# lib-wasi-littlefs

set(lib_wasi_littlefs_sources
	"wasi_vfs_impl_littlefs.c"
	"wasi_littlefs.c"
	"wasi_littlefs_impl.c"
	"wasi_littlefs_mount.c"
	"wasi_littlefs_ops.c"
)

set(lib_wasi_littlefs_headers
	"wasi_littlefs.h"
)

if(NOT DEFINED TOYWASM_LITTLEFS_SOURCE_DIR)
set(TOYWASM_LITTLEFS_SOURCE_DIR "littlefs" CACHE STRING "littlefs source path")
endif()

# TOYWASM_LITTLEFS_PREFIX is intended to be used with
# littlefs' scripts/changeprefix.py script.
if(NOT DEFINED TOYWASM_LITTLEFS_PREFIX)
set(TOYWASM_LITTLEFS_PREFIX "lfs" CACHE STRING "littlefs symbol prefix")
endif()
string(TOUPPER ${TOYWASM_LITTLEFS_PREFIX} TOYWASM_LITTLEFS_PREFIX_UPPER)

if(TOYWASM_LITTLEFS_PREFIX STREQUAL "lfs")
unset(TOYWASM_LITTLEFS_USE_PREFIX)
else()
set(TOYWASM_LITTLEFS_USE_PREFIX 1)
endif()

configure_file("wasi_littlefs_config.h.in"
    "${CMAKE_BINARY_DIR}/wasi_littlefs_config.h")
include_directories(${CMAKE_BINARY_DIR})

set(littlefs_sources
    "${TOYWASM_LITTLEFS_SOURCE_DIR}/${TOYWASM_LITTLEFS_PREFIX}.c"
    "${TOYWASM_LITTLEFS_SOURCE_DIR}/${TOYWASM_LITTLEFS_PREFIX}_util.c"
)

add_library(toywasm-lib-wasi-littlefs STATIC ${lib_wasi_littlefs_sources} ${littlefs_sources})
set_target_properties(toywasm-lib-wasi-littlefs PROPERTIES OUTPUT_NAME toywasm-wasi-littlefs)
if (USE_IPO)
# Note: -flto=full seems to produce considerably faster code
# than -flto=thin for us. However, cmake INTERPROCEDURAL_OPTIMIZATION
# always use -flto=thin for clang.
# cf. https://gitlab.kitware.com/cmake/cmake/-/issues/16808
set_property(TARGET toywasm-lib-wasi-littlefs PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
set_property(TARGET toywasm-lib-wasi-littlefs PROPERTY PUBLIC_HEADER ${lib_wasi_littlefs_headers})
target_link_libraries(toywasm-lib-wasi-littlefs toywasm-lib-wasi)
target_include_directories(toywasm-lib-wasi-littlefs
                           INTERFACE
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                           $<INSTALL_INTERFACE:include>
                           PRIVATE ${TOYWASM_LITTLEFS_SOURCE_DIR})
target_compile_definitions(toywasm-lib-wasi-littlefs PRIVATE
		${TOYWASM_LITTLEFS_PREFIX_UPPER}_THREADSAFE
		${TOYWASM_LITTLEFS_PREFIX_UPPER}_MULTIVERSION)
if (NOT CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(toywasm-lib-wasi-littlefs PRIVATE ${TOYWASM_LITTLEFS_PREFIX_UPPER}_NO_DEBUG ${TOYWASM_LITTLEFS_PREFIX_UPPER}_NO_ASSERT)
endif()

install(TARGETS toywasm-lib-wasi-littlefs
        EXPORT toywasm-lib-wasi-littlefs-config
        PUBLIC_HEADER DESTINATION include/toywasm)
install(EXPORT toywasm-lib-wasi-littlefs-config
        DESTINATION lib/cmake/toywasm-lib-wasi-littlefs)
install(FILES ${TOYWASM_LITTLEFS_SOURCE_DIR}/LICENSE.md DESTINATION share/doc/toywasm/littlefs)
