# Include header directories.
#file(GLOB TASM_INCLUDE_DIRS "*/include")
message("Include dirs: ${TASM_INCLUDE_DIRS} ${STITCHING_INCLUDE_DIRS}")
include_directories(${TASM_INCLUDE_DIRS} ${STITCHING_INCLUDE_DIRS})

# Add sources.
file(GLOB TASM_SOURCE_DIRS "*/src")
message("Source dirs: ${TASM_SOURCE_DIRS}")
foreach(subdir ${TASM_SOURCE_DIRS})
    add_subdirectory(${subdir}/..)
endforeach()

# Gather the core source files
file(GLOB_RECURSE TASM_SOURCES "*.cc" "*.cu")
set(TASM_SOURCES ${TASM_SOURCES})
message("sources: ${TASM_SOURCES}")

# Compile sources with PIC
add_library(tasm OBJECT ${TASM_SOURCES})
set_property(TARGET tasm PROPERTY POSITION_INDEPENDENT_CODE ON)
# Set target architectures.
set_property(TARGET tasm PROPERTY CUDA_ARCHITECTURES OFF)

# Create static library
add_library(tasm_static STATIC $<TARGET_OBJECTS:tasm>)
set_target_properties(tasm_static PROPERTIES OUTPUT_NAME "tasm")
# Set target architectures.
set_property(TARGET tasm_static PROPERTY CUDA_ARCHITECTURES OFF)

# Create dynamic library
add_library(tasm_shared SHARED $<TARGET_OBJECTS:tasm>)
set_target_properties(tasm_shared PROPERTIES OUTPUT_NAME "tasm")
target_link_libraries(tasm_shared ${TASM_LIB_DEPENDENCIES})
# Set target architectures.
set_property(TARGET tasm_shared PROPERTY CUDA_ARCHITECTURES OFF)

# Install libraries
install(
        TARGETS tasm_shared tasm_static
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)
file(GLOB_RECURSE TASM_HEADERS "include/*.h")
install(FILES ${TASM_HEADERS} DESTINATION include)

# Build python library.


