add_subdirectory(lib)

# the executable
add_executable(rag-bin main.c)
set_property(TARGET rag-bin PROPERTY C_EXTENSIONS off)
set_property(TARGET rag-bin PROPERTY OUTPUT_NAME rag)
target_link_libraries(rag-bin PRIVATE rag-lib raylib)

# the benchmark
add_executable(rag-bench bench.c)
set_property(TARGET rag-bench PROPERTY C_EXTENSIONS off)
target_link_libraries(rag-bench PRIVATE rag-lib)

# Vulkan playground
find_package(Vulkan REQUIRED)
add_executable(vk vk.c)
set_property(TARGET vk PROPERTY C_EXTENSIONS off)
target_link_libraries(vk PRIVATE Vulkan::Vulkan rag-lib)
