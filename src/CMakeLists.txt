add_subdirectory(lib)

# executable
add_executable(nbody-bin main.c)
target_link_libraries(nbody-bin PRIVATE nbody-lib raylib)
set_target_properties(nbody-bin PROPERTIES
        C_EXTENSIONS off
        OUTPUT_NAME nbody)

# benchmark uses clock_gettime which is not available on Windows and MacOS
if (UNIX AND NOT APPLE)
    add_executable(nbody-bench bench.c)
    target_link_libraries(nbody-bench PRIVATE nbody-lib)
endif ()
