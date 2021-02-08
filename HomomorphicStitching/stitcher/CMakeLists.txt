project(HomomorphicStitching)

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)

add_executable(stitcher main.cpp)

message(${CMAKE_SOURCE_DIR})

set(STITCHING_LIB_DIR ../../homomorphic_stitching)
add_subdirectory(${STITCHING_LIB_DIR} homomorphic_stitching)
include_directories(${STITCHING_LIB_DIR}/include)
target_link_libraries(stitcher homomorphic_stitching)
