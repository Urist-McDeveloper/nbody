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

# the executable
add_executable(rag src/main.c src/body.c)
target_compile_options(rag PRIVATE -Wall -Wextra -Werror)

target_include_directories(rag PUBLIC include)
target_link_libraries(rag PRIVATE OpenMP::OpenMP_C m raylib)
