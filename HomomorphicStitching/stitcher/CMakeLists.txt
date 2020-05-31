project(LightDB)

cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)
#add_subdirectory(lib/googletest-master)
#include_directories(lib/googletest-master/googletest/include)
#include_directories(lib/googletest-master/googlemock/include)

add_executable(stitcher main.cpp SliceSegmentLayer.cpp SliceSegmentLayer.h Stitcher.cpp Stitcher.h Opaque.cpp Opaque.h Nal.cpp Nal.h Headers.h Headers.cpp BitStream.h PictureParameterSet.h VideoParameterSet.h BitStream.cpp NalType.h BitArray.h PictureParameterSet.cpp VideoParameterSet.cpp Profile.h Profile.cpp SequenceParameterSet.h SequenceParameterSet.cpp Emulation.h Emulation.cpp BitArray.cpp AccessDelimiter.cpp AccessDelimiter.h Context.cpp Context.h Golombs.cpp Golombs.h)
target_link_libraries(stitcher)
