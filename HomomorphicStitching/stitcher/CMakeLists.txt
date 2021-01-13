project(LightDB)

cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 17)
#add_subdirectory(lib/googletest-master)
#include_directories(lib/googletest-master/googletest/include)
#include_directories(lib/googletest-master/googlemock/include)

set(STITCHER_FILES SliceSegmentLayer.cpp SliceSegmentLayer.h TileStitcher.cpp TileStitcher.h Opaque.cpp Opaque.h Nal.cpp Nal.h Headers.h Headers.cpp BitStream.h PictureParameterSet.h VideoParameterSet.h BitStream.cpp NalType.h BitArray.h PictureParameterSet.cpp VideoParameterSet.cpp Profile.h Profile.cpp SequenceParameterSet.h SequenceParameterSet.cpp Emulation.h Emulation.cpp BitArray.cpp AccessDelimiter.cpp AccessDelimiter.h Context.cpp Context.h Golombs.cpp Golombs.h)

#add_executable(stitcher main.cpp ${STITCHER_FILES})
#target_link_libraries(stitcher)

add_library(stitcherlib ${STITCHER_FILES})
