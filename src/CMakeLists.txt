add_subdirectory(lib)

# executable
add_executable(nbody main.c)
target_link_libraries(nbody PRIVATE nbody-lib raylib)
target_compile_options(nbody PRIVATE ${nbody_compiler_flags})
set_target_properties(nbody PROPERTIES C_EXTENSIONS off)

# benchmark uses clock_gettime which is not available on Windows and MacOS
if (UNIX AND NOT APPLE)
    add_executable(nbody-bench bench.c)
    target_link_libraries(nbody-bench PRIVATE nbody-lib)
    target_compile_options(nbody-bench PRIVATE ${nbody_compiler_flags})
endif()
