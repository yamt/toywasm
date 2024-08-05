# lib-dyld

set(lib_dyld_sources
	"dyld.c"
	"dyld_plt.c"
	"dyld_stats.c"
)

if(TOYWASM_ENABLE_DYLD_DLFCN)
list(APPEND lib_dyld_sources
	"dyld_dlfcn.c")
endif()

set(lib_dyld_headers
	"dyld.h"
)

add_library(toywasm-lib-dyld STATIC ${lib_dyld_sources})
set_target_properties(toywasm-lib-dyld PROPERTIES OUTPUT_NAME toywasm-dyld)
if (USE_IPO)
# Note: -flto=full seems to produce considerably faster code
# than -flto=thin for us. However, cmake INTERPROCEDURAL_OPTIMIZATION
# always use -flto=thin for clang.
# cf. https://gitlab.kitware.com/cmake/cmake/-/issues/16808
set_property(TARGET toywasm-lib-dyld PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
set_property(TARGET toywasm-lib-dyld PROPERTY PUBLIC_HEADER ${lib_dyld_headers})
target_link_libraries(toywasm-lib-dyld toywasm-lib-core)
target_include_directories(toywasm-lib-dyld
                           INTERFACE
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                           $<INSTALL_INTERFACE:include>)

install(TARGETS toywasm-lib-dyld
        EXPORT toywasm-lib-dyld-config
        PUBLIC_HEADER DESTINATION include/toywasm)
install(EXPORT toywasm-lib-dyld-config
        DESTINATION lib/cmake/toywasm-lib-dyld)
