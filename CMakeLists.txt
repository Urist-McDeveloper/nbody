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
        src/quadtree.c
        src/world.c)

set(rag_headers
        include/body.h
        include/err.h
        include/v2.h
        include/quadtree.h
        include/world.h)

# the library
add_library(rag STATIC ${rag_sources} ${rag_headers})
target_include_directories(rag PUBLIC include)
target_compile_options(rag PUBLIC -Wall -Wextra -Werror)
target_link_libraries(rag PUBLIC m OpenMP::OpenMP_C)

# the executable
add_executable(rag-bin src/main.c)
set_target_properties(rag-bin PROPERTIES OUTPUT_NAME rag)
target_link_libraries(rag-bin PRIVATE rag raylib)
