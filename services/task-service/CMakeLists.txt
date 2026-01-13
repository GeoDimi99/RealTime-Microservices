cmake_minimum_required(VERSION 3.20)
project(task_service C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add executable
add_executable(task-service
    src/main.c
    src/logger.c  # directly add logger.c to build
    src/task_ipc.c
)

# Threads
find_package(Threads REQUIRED)
target_link_libraries(task-service PRIVATE Threads::Threads)


# Default queue prefix if not passed
if(NOT DEFINED TASK_QUEUE_PREFIX)
    set(TASK_QUEUE_PREFIX "/task")
endif()

# Generate header from template
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/task_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/task_config.h
    @ONLY
)

# Include generated header
include_directories(${CMAKE_CURRENT_BINARY_DIR})
