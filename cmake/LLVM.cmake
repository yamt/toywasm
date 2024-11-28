if(NOT CMAKE_TOOLCHAIN_FILE)
# Prefer the homebrew version because xcode clang doesn't have detect_leaks
# Note: CMAKE_SYSTEM_NAME is not available yet.
if(NOT DEFINED CUSTOM_LLVM_HOME)
# Note: The recent macOS SDKs (eg. math.h from macOS SDK 15) assume that
# _Float16 is available. In case of x86, it's only available for the
# recent versions of the ABI. In case of LLVM, the support has been added
# for LLVM>=15.
set(CUSTOM_LLVM_HOME /usr/local/opt/llvm@15)
endif()
endif()

if(DEFINED CUSTOM_LLVM_HOME)
find_program(CUSTOM_CLANG ${CUSTOM_LLVM_HOME}/bin/clang)
find_program(CUSTOM_CLANGXX ${CUSTOM_LLVM_HOME}/bin/clang++)
if(CUSTOM_CLANG)
set(CMAKE_C_COMPILER ${CUSTOM_CLANG} CACHE FILEPATH "brew llvm clang" FORCE)
if(CUSTOM_CLANGXX)
set(CMAKE_CXX_COMPILER ${CUSTOM_CLANGXX} CACHE FILEPATH "brew llvm clang++" FORCE)
endif()
find_program(CUSTOM_AR ${CUSTOM_LLVM_HOME}/bin/llvm-ar REQUIRED)
set(CMAKE_AR ${CUSTOM_AR} CACHE FILEPATH "custom llvm ar" FORCE)
set(CMAKE_C_COMPILER_AR ${CUSTOM_AR} CACHE FILEPATH "custom llvm ar" FORCE)
find_program(CUSTOM_RANLIB ${CUSTOM_LLVM_HOME}/bin/llvm-ranlib REQUIRED)
set(CMAKE_RANLIB ${CUSTOM_RANLIB} CACHE FILEPATH "custom llvm ar" FORCE)
set(CMAKE_C_COMPILER_RANLIB ${CUSTOM_RANLIB} CACHE FILEPATH "custom llvm ranlib" FORCE)
endif()
endif()
