add_library(pyTasm SHARED src/bindings.cpp)

find_package(Python2 COMPONENTS Development)
include_directories(${Python2_INCLUDE_DIRS})

find_package(Boost 1.64 COMPONENTS python filesystem system REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

include_directories(${TASM_INCLUDE_DIRS})

target_link_libraries(pyTasm ${Boost_LIBRARIES} ${Python2_LIBRARIES} tasm)

# Suppress prefix "lib" because Python does not allow this prefix
set_target_properties(pyTasm PROPERTIES PREFIX "")
