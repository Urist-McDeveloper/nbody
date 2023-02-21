cmake_minimum_required(VERSION 3.18)

project(rag C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE true)

add_compile_options(-Wall -Wextra -Werror)

find_library(REQUIRED m)
find_library(REQUIRED raylib)
find_package(OpenMP REQUIRED)

add_executable(rag src/main.c src/body.c)
target_include_directories(rag PRIVATE include)

target_link_libraries(rag PRIVATE OpenMP::OpenMP_C m raylib)
