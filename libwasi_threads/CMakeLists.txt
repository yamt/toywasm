# lib-wasi-threads

set(lib_wasi_threads_sources
	"wasi_threads.c"
)

set(lib_wasi_threads_headers
	"wasi_threads.h"
)

add_library(toywasm-lib-wasi-threads STATIC ${lib_wasi_threads_sources})
set_target_properties(toywasm-lib-wasi-threads PROPERTIES OUTPUT_NAME toywasm-wasi-threads)
if (USE_IPO)
# Note: -flto=full seems to produce considerably faster code
# than -flto=thin for us. However, cmake INTERPROCEDURAL_OPTIMIZATION
# always use -flto=thin for clang.
# cf. https://gitlab.kitware.com/cmake/cmake/-/issues/16808
set_property(TARGET toywasm-lib-wasi-threads PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
set_property(TARGET toywasm-lib-wasi-threads PROPERTY PUBLIC_HEADER ${lib_wasi_threads_headers})
target_link_libraries(toywasm-lib-wasi-threads toywasm-lib-wasi)
target_include_directories(toywasm-lib-wasi-threads
                           INTERFACE
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                           $<INSTALL_INTERFACE:include>)

install(TARGETS toywasm-lib-wasi-threads
        EXPORT toywasm-lib-wasi-threads-config
        PUBLIC_HEADER DESTINATION include/toywasm)
install(EXPORT toywasm-lib-wasi-threads-config
        DESTINATION lib/cmake/toywasm-lib-wasi-threads)
