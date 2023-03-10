add_subdirectory(lib)
add_subdirectory(shader)

# the executable
add_executable(rag-bin main.c)
target_link_libraries(rag-bin PRIVATE rag-lib raylib)

set_target_properties(rag-bin PROPERTIES
        C_EXTENSIONS off
        OUTPUT_NAME rag)

# the benchmark
add_executable(rag-bench bench.c)
target_link_libraries(rag-bench PRIVATE rag-lib)

set_property(TARGET rag-bench PROPERTY C_EXTENSIONS off)

# Vulkan playground
find_package(Vulkan REQUIRED)

add_executable(vk vk.c)
target_link_libraries(vk PRIVATE Vulkan::Vulkan rag-lib)

set_property(TARGET vk PROPERTY C_EXTENSIONS off)
compile_shaders(vk
        STAGE comp
        DEPENDS shader/body_common_cs.glsl
        SOURCE shader/body_grav_cs.glsl shader/body_move_cs.glsl)

