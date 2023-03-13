find_package(OpenMP)
find_package(Vulkan REQUIRED)
find_library(REQUIRED m)

set(nbody_lib_headers
        ${CMAKE_SOURCE_DIR}/include/nbody.h
        ${CMAKE_SOURCE_DIR}/include/nbody_vk.h)
set(nbody_lib_sources
        fio.c
        particle_pack.c
        vk_ctx.c
        world.c
        world_vk.c)

add_library(nbody-lib STATIC ${nbody_lib_sources} ${nbody_lib_headers})
target_include_directories(nbody-lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(nbody-lib PUBLIC Vulkan::Vulkan)

if (USE_AVX)
    target_compile_options(nbody-lib PUBLIC -mavx)
    target_compile_definitions(nbody-lib PUBLIC USE_AVX)
endif()

if (OpenMP_C_FOUND)
    target_link_libraries(nbody-lib PRIVATE OpenMP::OpenMP_C)
endif()

compile_shaders(nbody-lib STAGE comp SOURCE ../shader/particle_cs.glsl)

set_target_properties(nbody-lib PROPERTIES
        C_EXTENSIONS off
        OUTPUT_NAME nbody)
