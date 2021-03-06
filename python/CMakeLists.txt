add_library(_tasm SHARED src/bindings.cpp)

find_package(Python3 COMPONENTS Development)
include_directories(${Python3_INCLUDE_DIRS})

find_package(Boost 1.75 REQUIRED COMPONENTS python36 numpy36)
message("Python directories: ${Boost_INCLUDE_DIR} ${Boost_LIBRARIES} ${Python3_LIBRARIES}")
include_directories(${Boost_INCLUDE_DIR})

include_directories(${TASM_INCLUDE_DIRS} include)

target_link_libraries(_tasm ${Boost_LIBRARIES} ${Python3_LIBRARIES} ${TASM_LIB_DEPENDENCIES} tasm)

# Suppress prefix "lib" because Python does not allow this prefix
set_target_properties(_tasm PROPERTIES PREFIX "")
set_target_properties(_tasm PROPERTIES LIBRARY_OUTPUT_DIRECTORY python_package/tasm)
# Set target architectures.
set_property(TARGET _tasm PROPERTY CUDA_ARCHITECTURES OFF)

file(COPY python_package DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

