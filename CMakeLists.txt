cmake_minimum_required(VERSION 3.28.3)
project(microbus)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_BUILD_TYPE Debug)

# set(EXECUTABLE_OUTPUT_PATH bin)

# Grab all source and test files - a bit messy but as this is a small project let's just keep it simple for now
FILE(GLOB TEST_SOURCES
        "src/*.c"
        "src/*.h"
        "test/*.c"
        "test/*.h")

list(FILTER TEST_SOURCES EXCLUDE REGEX "src/stm32F1SpiMaster.c")
list(FILTER TEST_SOURCES EXCLUDE REGEX "src/stm32G0SpiNode.c")

# Enable testing
enable_testing()

add_executable(MyTest ${TEST_SOURCES})

# Add tests
add_test(NAME MyTest COMMAND MyTest)
