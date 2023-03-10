add_subdirectory(lib)

# executable
add_executable(rag-bin main.c)
target_link_libraries(rag-bin PRIVATE rag-lib raylib)

set_target_properties(rag-bin PROPERTIES
        C_EXTENSIONS off
        OUTPUT_NAME rag)

# benchmark
add_executable(rag-bench bench.c)
target_link_libraries(rag-bench PRIVATE rag-lib)
set_property(TARGET rag-bench PROPERTY C_EXTENSIONS off)
