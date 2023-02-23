# lib-core

set(lib_core_sources
	"bitmap.c"
	"cell.c"
	"context.c"
	"decode.c"
	"endian.c"
	"exec.c"
	"expr.c"
	"fileio.c"
	"host_instance.c"
	"idalloc.c"
	"import_object.c"
	"insn.c"
	"instance.c"
	"leb128.c"
	"list.c"
	"module.c"
	"nbio.c"
	"report.c"
	"shared_memory.c"
	"timeutil.c"
	"type.c"
	"util.c"
	"validation.c"
	"vec.c"
	"xlog.c"
	"${CMAKE_BINARY_DIR}/toywasm_config.c"
)

if(TOYWASM_ENABLE_WASM_THREADS)
list(APPEND lib_core_sources
	"cluster.c"
	"waitlist.c")
if(TOYWASM_USE_USER_SCHED)
list(APPEND lib_core_sources
	"usched.c")
else()
list(APPEND lib_core_sources
	"lock.c")
endif()
endif()

if(TOYWASM_ENABLE_WRITER)
set(lib_core_sources_writer
	"module_writer.c"
)
endif()

set(lib_core_headers
	"bitmap.h"
	"cell.h"
	"context.h"
	"fileio.h"
	"idalloc.h"
	"instance.h"
	"list.h"
	"load_context.h"
	"lock.h"
	"module.h"
	"module_writer.h"
	"nbio.h"
	"options.h"
	"platform.h"
	"report.h"
	"type.h"
	"usched.h"
	"util.h"
	"vec.h"
	"xlog.h"
	"${CMAKE_BINARY_DIR}/toywasm_config.h"
	"${CMAKE_BINARY_DIR}/toywasm_version.h"
)

add_library(toywasm-lib-core STATIC ${lib_core_sources} ${lib_core_sources_writer})
set_target_properties(toywasm-lib-core PROPERTIES OUTPUT_NAME toywasm-core)
if (USE_IPO)
# Note: -flto=full seems to produce considerably faster code
# than flto=thin for us. However, cmake INTERPROCEDURAL_OPTIMIZATION
# always use -flto=thin for clang.
# cf. https://gitlab.kitware.com/cmake/cmake/-/issues/16808
set_property(TARGET toywasm-lib-core PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
set_property(TARGET toywasm-lib-core PROPERTY PUBLIC_HEADER ${lib_core_headers})
if(TOYWASM_USE_SHORT_ENUMS)
target_compile_options(toywasm-lib-core PUBLIC "-fshort-enums")
endif()
target_include_directories(toywasm-lib-core
                           INTERFACE
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                           $<INSTALL_INTERFACE:include>)

install(TARGETS toywasm-lib-core
        EXPORT toywasm-lib-core-config
        PUBLIC_HEADER DESTINATION include/toywasm)
install(EXPORT toywasm-lib-core-config
        DESTINATION lib/cmake/toywasm-lib-core)

