# lib-core

set(lib_core_sources
	"bitmap.c"
	"cconv.c"
	"cell.c"
	"context.c"
	"decode.c"
	"endian.c"
	"exec.c"
	"exec_debug.c"
	"exec_insn_subr.c"
	"expr.c"
	"expr_parser.c"
	"fileio.c"
	"host_instance.c"
	"idalloc.c"
	"import_object.c"
	"insn.c"
	"instance.c"
	"leb128.c"
	"list.c"
	"load_context.c"
	"mem.c"
	"module.c"
	"name.c"
	"nbio.c"
	"options.c"
	"report.c"
	"restart.c"
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
	"suspend.c"
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
	"cconv.h"
	"cell.h"
	"context.h"
	"dylink_type.h"
	"endian.h"
	"exec_context.h"
	"exec_debug.h"
	"expr_parser.h"
	"fileio.h"
	"host_instance.h"
	"idalloc.h"
	"instance.h"
	"leb128.h"
	"list.h"
	"load_context.h"
	"lock.h"
	"module.h"
	"module_writer.h"
	"name.h"
	"nbio.h"
	"options.h"
	"platform.h"
	"report.h"
	"restart.h"
	"type.h"
	"timeutil.h"
	"usched.h"
	"util.h"
	"valtype.h"
	"vec.h"
	"xlog.h"
	"${CMAKE_BINARY_DIR}/toywasm_config.h"
	"${CMAKE_BINARY_DIR}/toywasm_version.h"
)

add_library(toywasm-lib-core STATIC ${lib_core_sources} ${lib_core_sources_writer})
set_target_properties(toywasm-lib-core PROPERTIES OUTPUT_NAME toywasm-core)
if (USE_IPO)
# Note: -flto=full seems to produce considerably faster code
# than -flto=thin for us. However, cmake INTERPROCEDURAL_OPTIMIZATION
# always use -flto=thin for clang.
# cf. https://gitlab.kitware.com/cmake/cmake/-/issues/16808
set_property(TARGET toywasm-lib-core PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

# Note:
# clang-10 on ubuntu needs this to link IPO-using libraries.
# clang-11 on ubuntu doesn't seem to need this.
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
if (CMAKE_C_COMPILER_ID MATCHES "Clang")
target_link_options(toywasm-lib-core INTERFACE "-fuse-ld=lld")
endif() # Clang
endif() # Linux
endif() # USE_IPO

foreach(F IN LISTS SANITIZER_FLAGS)
    target_link_options(toywasm-lib-core INTERFACE ${F})
endforeach()
set_property(TARGET toywasm-lib-core PROPERTY PUBLIC_HEADER ${lib_core_headers})
if(TOYWASM_USE_SHORT_ENUMS)
target_compile_options(toywasm-lib-core PUBLIC "-fshort-enums")
endif()

# random things to experiment
#target_compile_options(toywasm-lib-core PUBLIC "SHELL:-Xclang -fmerge-functions" "SHELL:-mllvm -mergefunc-use-aliases")
#target_compile_options(toywasm-lib-core PUBLIC "SHELL:-mllvm -enable-machine-outliner")
#target_compile_options(toywasm-lib-core PUBLIC "SHELL:-mllvm -hot-cold-split")
#target_compile_options(toywasm-lib-core PUBLIC "SHELL:-mllvm --enable-gvn-hoist")
#target_compile_options(toywasm-lib-core PUBLIC "SHELL:-mllvm --enable-gvn-sink")

target_include_directories(toywasm-lib-core
                           INTERFACE
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                           $<INSTALL_INTERFACE:include>)

install(TARGETS toywasm-lib-core
        EXPORT toywasm-lib-core-config
        PUBLIC_HEADER DESTINATION include/toywasm)
install(EXPORT toywasm-lib-core-config
        DESTINATION lib/cmake/toywasm-lib-core)

