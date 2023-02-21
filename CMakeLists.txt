cmake_minimum_required(VERSION 3.18)

# general configuration
project(rag C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE true)

# compiled dependencies
add_subdirectory(deps/raylib)

# system dependencies
find_library(REQUIRED m)
find_package(OpenMP REQUIRED)

# the executable
add_executable(rag src/main.c src/body.c)
target_compile_options(rag PRIVATE -Wall -Wextra -Werror)

target_include_directories(rag PUBLIC include)
target_link_libraries(rag PRIVATE OpenMP::OpenMP_C m raylib)
