project (HomomorphicStitcher)

cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD  17)

file(GLOB_RECURSE STITCHER_SOURCES "*.cc")
message("Stitcher sources: ${STITCHER_SOURCES}")

include_directories("include/")

add_library(homomorphic_stitching ${STITCHER_SOURCES})
set_property(TARGET homomorphic_stitching PROPERTY POSITION_INDEPENDENT_CODE ON)
