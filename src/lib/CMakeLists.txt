find_package(OpenMP REQUIRED)
find_package(Vulkan REQUIRED)
find_library(REQUIRED m)

set(rag_lib_headers
        ${CMAKE_SOURCE_DIR}/include/rag.h
        ${CMAKE_SOURCE_DIR}/include/rag_vk.h)
set(rag_lib_sources
        body.c
        fio.c
        vk_ctx.c
        world.c)

add_library(rag-lib STATIC ${rag_lib_sources} ${rag_lib_headers})
target_include_directories(rag-lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(rag-lib PUBLIC OpenMP::OpenMP_C Vulkan::Vulkan m)

compile_shaders(rag-lib STAGE comp SOURCE ../shader/body_cs.glsl)

set_target_properties(rag-lib PROPERTIES
        C_EXTENSIONS off
        OUTPUT_NAME rag)
