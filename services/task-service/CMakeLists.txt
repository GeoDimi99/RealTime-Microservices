cmake_minimum_required(VERSION 3.20)
project(task_service C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/../common/include)

# Add executable
add_executable(task-service
    src/main.c
    ../common/src/logger.c  # directly add logger.c to build
)

# Threads
find_package(Threads REQUIRED)
target_link_libraries(task-service PRIVATE Threads::Threads)
