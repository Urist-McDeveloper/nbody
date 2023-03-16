add_subdirectory(lib)

# executable
find_library(REQUIRED m)

add_executable(nbody main.c cluster.c)
target_link_libraries(nbody PRIVATE m nbody-lib raylib)
target_compile_options(nbody PRIVATE ${nbody_compiler_flags})
set_target_properties(nbody PROPERTIES C_EXTENSIONS off)

# benchmark uses clock_gettime which is not available on Windows and MacOS
if (UNIX AND NOT APPLE)
    add_executable(nbody-bench bench.c cluster.c)
    target_link_libraries(nbody-bench PRIVATE m nbody-lib)
    target_compile_options(nbody-bench PRIVATE ${nbody_compiler_flags})
endif()
