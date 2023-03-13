find_package(OpenMP)
find_package(Vulkan REQUIRED)
find_library(REQUIRED m)

set(rag_lib_headers
        ${CMAKE_SOURCE_DIR}/include/rag.h
        ${CMAKE_SOURCE_DIR}/include/rag_vk.h)
set(rag_lib_sources
        fio.c
        particle_pack.c
        vk_ctx.c
        world.c
        world_vk.c)

add_library(rag-lib STATIC ${rag_lib_sources} ${rag_lib_headers})
target_include_directories(rag-lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(rag-lib PUBLIC Vulkan::Vulkan m)
target_compile_options(rag-lib PUBLIC -mavx)

if (OpenMP_C_FOUND)
    target_link_libraries(rag-lib PRIVATE OpenMP::OpenMP_C)
endif()

compile_shaders(rag-lib STAGE comp SOURCE ../shader/particle_cs.glsl)

set_target_properties(rag-lib PROPERTIES
        C_EXTENSIONS off
        OUTPUT_NAME rag)
