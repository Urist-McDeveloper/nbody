cmake_minimum_required(VERSION 3.18)
project(rag C)

# general configuration
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

# enable IPO on Release (if supported)
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if (ipo_supported)
    set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif ()

# packaged dependencies
add_subdirectory(deps)

# system dependencies
find_library(REQUIRED m)
find_package(OpenMP REQUIRED)

# project sources
set(rag_lib_sources
        src/lib/body.c
        src/lib/quadtree.c
        src/lib/world.c)

set(rag_lib_headers include/rag.h)

# the library
add_library(rag-lib STATIC ${rag_lib_sources} ${rag_lib_headers})
set_property(TARGET rag-lib PROPERTY C_EXTENSIONS off)
set_property(TARGET rag-lib PROPERTY OUTPUT_NAME rag)

target_include_directories(rag-lib PUBLIC include)
target_link_libraries(rag-lib PUBLIC OpenMP::OpenMP_C m)

if (MSVC)
    target_compile_options(rag-lib PUBLIC /Wall)
else()
    target_compile_options(rag-lib PUBLIC -Wall -Wextra -Werror)
endif()

# the executable
add_executable(rag-bin src/main.c)
set_property(TARGET rag-bin PROPERTY C_EXTENSIONS off)
set_property(TARGET rag-bin PROPERTY OUTPUT_NAME rag)
target_link_libraries(rag-bin PRIVATE rag-lib raylib)

# the benchmark
add_executable(rag-bench src/bench.c)
set_property(TARGET rag-bench PROPERTY C_EXTENSIONS off)
target_link_libraries(rag-bench PRIVATE rag-lib)

# the tests
add_subdirectory(test)
