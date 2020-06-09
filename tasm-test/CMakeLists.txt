enable_testing()

#include(${PROJECT_SOURCE_DIR}/cmake/gtest.cmake)

file(GLOB TASM_TEST_INCLUDE_DIRS "include/*")
include_directories(${TASM_TEST_INCLUDE_DIRS})

# Include TASM header directories
file(GLOB TASM_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/tasm/*/include/")
include_directories(${TASM_INCLUDE_DIRS})

file(GLOB_RECURSE TASM_TEST_SOURCES "src/*")
message("TASM_TEST_SOURCES: ${TASM_TEST_SOURCES}")

# Build tests
add_executable(tasm_test EXCLUDE_FROM_ALL ${TASM_TEST_SOURCES})
target_link_libraries(
        tasm_test   gtest #${GTEST_BOTH_LIBRARIES}
        tasm_static ${TASM_LIB_DEPENDENCIES}
)
add_test(tasm_test tasm_test)



