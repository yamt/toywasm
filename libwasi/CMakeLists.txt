# lib-wasi

set(lib_wasi_sources
	"wasi.c"
	"wasi_fdtable.c"
	"wasi_path_subr.c"
	"wasi_poll_subr.c"
	"wasi_subr.c"
	"wasi_host_pathop.c"
	"wasi_host_fdop.c"
	"wasi_host_dirent.c"
	"wasi_host_subr.c"

	"wasi_clock.c"
	"wasi_fd.c"
	"wasi_environ.c"
	"wasi_path.c"
	"wasi_poll.c"
	"wasi_proc.c"
	"wasi_random.c"
	"wasi_sched.c"
	"wasi_sock.c"
)

if(TOYWASM_ENABLE_WASI_THREADS)
list(APPEND lib_wasi_sources
	"wasi_threads.c")
endif()

set(lib_wasi_headers
	"wasi.h"
	"wasi_threads.h"
)

add_library(toywasm-lib-wasi STATIC ${lib_wasi_sources})
set_target_properties(toywasm-lib-wasi PROPERTIES OUTPUT_NAME toywasm-wasi)
if (USE_IPO)
# Note: -flto=full seems to produce considerably faster code
# than -flto=thin for us. However, cmake INTERPROCEDURAL_OPTIMIZATION
# always use -flto=thin for clang.
# cf. https://gitlab.kitware.com/cmake/cmake/-/issues/16808
set_property(TARGET toywasm-lib-wasi PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
set_property(TARGET toywasm-lib-wasi PROPERTY PUBLIC_HEADER ${lib_wasi_headers})
target_link_libraries(toywasm-lib-wasi toywasm-lib-core)
target_include_directories(toywasm-lib-wasi
                           INTERFACE
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                           $<INSTALL_INTERFACE:include>)

install(TARGETS toywasm-lib-wasi
        EXPORT toywasm-lib-wasi-config
        PUBLIC_HEADER DESTINATION include/toywasm)
install(EXPORT toywasm-lib-wasi-config
        DESTINATION lib/cmake/toywasm-lib-wasi)
