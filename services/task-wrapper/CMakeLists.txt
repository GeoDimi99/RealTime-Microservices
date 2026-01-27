cmake_minimum_required(VERSION 3.20)
project(task_wrapper C)


set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

add_compile_definitions(_GNU_SOURCE)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add executable
add_executable(task-wrapper
    src/main.c
    src/logger.c  # directly add logger.c to build
    src/task_ipc.c
    src/task_wrapper.c
    src/task.c
)

# Threads
find_package(Threads REQUIRED)
target_link_libraries(task-wrapper PRIVATE Threads::Threads)


