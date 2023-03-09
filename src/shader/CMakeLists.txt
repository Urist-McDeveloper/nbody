find_package(Vulkan COMPONENTS glslc)
find_program(GLSLC NAMES glslc HINTS Vulkan::glslc)

set(COMMON_GLSL ${CMAKE_CURRENT_LIST_DIR}/common.glsl)

function(compile_shaders target)
    foreach(source ${ARGN})
        set(spv ${source}.spv)
        list(APPEND spv_all ${spv})

        add_custom_command(
                OUTPUT ${spv}
                DEPENDS ${source}
                DEPENDS ${COMMON_GLSL}
                COMMAND ${GLSLC} -o ${spv} ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        )
        target_sources(${target} PRIVATE ${source}.spv)
    endforeach()
endfunction()
