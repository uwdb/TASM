project(LightDB)
cmake_minimum_required(VERSION 3.10)

SET(BOOST_MIN_VERSION "1.64.0")

# Set the cmake Module path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

set(CMAKE_CUDA_COMPILER /usr/local/cuda/bin/nvcc)
enable_language(CUDA)

# Set shared library suffix
if(APPLE)
  set(SHARED_LIB_SUFFIX ".dylib")
else()
  set(SHARED_LIB_SUFFIX ".so")
endif()

# Only for Mac OS X warnings
if(APPLE)
  set(CMAKE_MACOSX_RPATH 1)
endif()

# Default user definitions
set(GTEST_FILTER "*" CACHE STRING "Filters for the Google unit tests.")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Set C++ standards
set (CMAKE_CXX_STANDARD 17)
#set (CLANG_TIDY_CXX_STANDARD 14)

# Set compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG -g3 -rdynamic -Wall -Wextra -Wno-unused-parameter -pg")
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3 -fvisibility=hidden -Wall  -Wextra -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS_COVERAGE "-DDEBUG --coverage -g3 -Wall -Wextra -Wno-unused-parameter")

set(TASM_LIB_DEPENDENCIES ${TASM_LIB_DEPENDENCIES} nvcuvid dl pthread stdc++fs)

include(cmake/nvenc.cmake)

find_package(CUDA 8.0 REQUIRED)
# Using SYSTEM breaks building.
include_directories(${CUDA_INCLUDE_DIRS})
message("Cuda include: ${CUDA_INCLUDE_DIRS}")
message("libs: ${CUDA_CUDA_LIBRARY}, ${CUDA_nvcuvenc_LIBRARY}")
set(TASM_LIB_DEPENDENCIES ${TASM_LIB_DEPENDENCIES} ${CUDA_CUDA_LIBRARY} ${CUDA_nvcuvenc_LIBRARY} /usr/lib/x86_64-linux-gnu/libnvcuvid.so /usr/local/cuda-9.0/lib64/libnvToolsExt.so)

find_package(PythonLibs 3)
include_directories(SYSTEM ${PYTHON_INCLUDE_DIRS})

find_package(Boost 1.64 COMPONENTS python filesystem system REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})
set(LIGHTDB_LIB_DEPENDENCIES ${LIGHTDB_LIB_DEPENDENCIES} ${Boost_LIBRARIES})

find_package(FFmpeg REQUIRED COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE)
include_directories(SYSTEM ${FFMPEG_INCLUDE_DIRS})
set(TASM_LIB_DEPENDENCIES ${TASM_LIB_DEPENDENCIES} ${FFMPEG_LIBRARIES})

# Protobufs
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIR})
include_directories(SYSTEM ${CMAKE_BINARY_DIR}/protobuf)
set(TASM_LIB_DEPENDENCIES ${TASM_LIB_DEPENDENCIES} ${Protobuf_LIBRARIES} lightdb_protobuf)
add_subdirectory(protobuf)

find_package (Glog REQUIRED)

find_package(GPAC REQUIRED)
include_directories(SYSTEM ${GPAC_INCLUDE_DIRS})
set(TASM_LIB_DEPENDENCIES ${TASM_LIB_DEPENDENCIES} ${GPAC_LIBRARIES})

find_library(SQLITE3_LIBRARY sqlite3)
set(TASM_LIB_DEPENDENCIES ${TASM_LIB_DEPENDENCIES} ${SQLITE3_LIBRARY})

# Enable testing
option(ENABLE_TESTS "Enable tests" ON)
if (${ENABLE_TESTS})
  enable_testing()
  add_subdirectory(test)
endif()

file(GLOB TASM_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/tasm/*/include")

add_subdirectory(tasm)
add_subdirectory(python)
enable_testing()
add_subdirectory(tasm-test)

# Add clang-format target
file(GLOB_RECURSE FORMATTED_SOURCE_FILES *.cc *.h)
add_custom_target(
  clang-format
  COMMAND /usr/bin/clang-format
  -style=file
  -i
  ${FORMATTED_SOURCE_FILES}
)

# Uninstall
set(CMD "xargs printf '-- Uninstalling: %s\\\\n' <install_manifest.txt")
add_custom_target(
   uninstall
   COMMAND eval "${CMD}"
   COMMAND xargs rm -f < install_manifest.txt
   COMMAND echo "TASM uninstalled"
)
