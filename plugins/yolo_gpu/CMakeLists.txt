cmake_minimum_required(VERSION 3.8)
project(yologpu)

set(CMAKE_CXX_STANDARD 17)

include_directories(${LIGHTDB_INCLUDES})

# NPP
include_directories(/usr/local/cuda/include)
link_directories(/usr/local/cuda/lib64)

# GPU Yolo linking
set(YOLO_GPU_DIR "/home/maureen/yolo_gpu_lightdb/darknet")
#file(GLOB YOLO_GPU_SOURCE_DIRS ${YOLO_GPU_DIR}/src)
file(GLOB YOLO_GPU_INCLUDE_DIRS ${YOLO_GPU_DIR}/include)
include_directories(${YOLO_GPU_INCLUDE_DIRS})
file(GLOB YOLO_GPU_LIBS ${YOLO_GPU_DIR}/*.so)
message("YOLO include dirs: ${YOLO_GPU_INCLUDE_DIRS}, lib: ${YOLO_GPU_LIBS}")
set(YOLO_GPU_LIB_DEPENDENCIES ${LIGHTDB_LIB_DEPENDENCIES} ${YOLO_GPU_LIBS})
link_directories(${YOLO_GPU_DIR})

add_library(yologpu SHARED udf.cc udf.h)
target_link_libraries(yologpu ${YOLO_GPU_LIB_DEPENDENCIES} nppicc nppig nppidei nppial)

ADD_CUSTOM_COMMAND(TARGET yologpu
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:yologpu> ${CMAKE_INSTALL_PREFIX})