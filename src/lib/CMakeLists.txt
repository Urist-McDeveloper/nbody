find_package(OpenMP)
find_package(Vulkan REQUIRED)
find_library(REQUIRED m)

set(nbody_lib_headers
        ${CMAKE_SOURCE_DIR}/include/nbody.h
        ${CMAKE_SOURCE_DIR}/include/galaxy.h)
set(nbody_lib_sources
        fio.c
        galaxy.c
        sim_cpu.c
        sim_gpu.c
        vulkan_ctx.c
        world.c)

add_library(nbody-lib STATIC ${nbody_lib_sources} ${nbody_lib_headers})
set_target_properties(nbody-lib PROPERTIES
        OUTPUT_NAME nbody)

target_include_directories(nbody-lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_compile_options(nbody-lib PRIVATE ${nbody_compiler_flags})
target_link_libraries(nbody-lib PUBLIC Vulkan::Vulkan m)

if (SIMD_SET STREQUAL "AVX")
    target_compile_definitions(nbody-lib PUBLIC USE_AVX)
    if (MSVC)
        target_compile_options(nbody-lib PUBLIC /arch:avx)
    else()
        target_compile_options(nbody-lib PUBLIC -mavx)
    endif()
elseif (SIMD_SET STREQUAL "SSE")
    target_compile_definitions(nbody-lib PUBLIC USE_SSE)
endif()

if (OpenMP_C_FOUND)
    target_link_libraries(nbody-lib PUBLIC OpenMP::OpenMP_C)
endif()

compile_shaders(nbody-lib STAGE comp SOURCE ../shader/particle_cs.glsl)
