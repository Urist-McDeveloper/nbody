cmake_minimum_required(VERSION 3.18)

# general configuration
project(rag C)
set(CMAKE_C_STANDARD 17)

# enable IPO on Release (if supported)
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if (ipo_supported)
    set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif ()

# compiled dependencies
set(OpenGL_GL_PREFERENCE GLVND)
add_subdirectory(deps/raylib)

# system dependencies
find_library(REQUIRED m)
find_package(OpenMP REQUIRED)

# project sources
set(rag_sources
        src/body.c
        src/main.c
        src/quadtree.c
        src/world.c)

set(rag_headers
        include/body.h
        include/v2.h
        include/quadtree.h
        include/world.h)

# the executable
add_executable(rag ${rag_sources} ${rag_headers})
target_compile_options(rag PRIVATE -Wall -Wextra -Werror)

target_include_directories(rag PUBLIC include)
target_link_libraries(rag PRIVATE OpenMP::OpenMP_C m raylib)
