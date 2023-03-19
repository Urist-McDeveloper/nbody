cmake_minimum_required(VERSION 3.20)
project(nbody C)

set(SIMD_SET "AVX" CACHE STRING "Which SIMD instruction set to use: AVX, SSE or none")

if (MSVC)
    set(nbody_compiler_flags /W4)
else()
    set(nbody_compiler_flags -Wall -Wextra)
endif()

add_subdirectory(deps)
add_subdirectory(src)
add_subdirectory(test)
