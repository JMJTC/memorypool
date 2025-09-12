# CMake minimum version
cmake_minimum_required(VERSION 3.10)

# Project name
project(memorypool_v2)

# Set c++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/centralcache.cpp
    src/pagecache.cpp
    src/threadcache.cpp
)
set(TEST_SOURCES
    test/memorypool_test.cpp
)

# Add the executable for the main target
add_library(${PROJECT_NAME}_shared SHARED ${SOURCES})
add_executable(${PROJECT_NAME}_test ${TEST_SOURCES} ${SOURCES})

# Link libraries if needed (e.g., pthread)
find_package(Threads REQUIRED)

if(UNIX)
    target_compile_options(${PROJECT_NAME}_shared PRIVATE -g -pthread)
    target_compile_options(${PROJECT_NAME}_test PRIVATE -g -pthread)
elseif(WIN32)
    target_compile_options(${PROJECT_NAME}_shared PRIVATE /Zi /W4)
    target_compile_options(${PROJECT_NAME}_test PRIVATE /Zi /W4)
endif()

target_link_libraries(${PROJECT_NAME}_shared PRIVATE Threads::Threads)
target_link_libraries(${PROJECT_NAME}_test PRIVATE Threads::Threads)
