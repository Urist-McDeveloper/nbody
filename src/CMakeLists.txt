find_package(Vulkan COMPONENTS glslc)
find_program(GLSLC NAMES glslc)

set(GLSLC_ENV "vulkan1.0" CACHE STRING "glslc --target-env=")

function(compile_shaders target stage)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "STAGE;DEPENDS" "SOURCE")
    foreach (source ${arg_SOURCE})
        cmake_path(REPLACE_EXTENSION source LAST_ONLY spv OUTPUT_VARIABLE spv)
        cmake_path(GET spv PARENT_PATH spv_dir)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${spv_dir})

        add_custom_command(
                OUTPUT ${spv}
                DEPENDS ${source} ${arg_DEPENDS}
                COMMAND
                    ${GLSLC} -O -o ${spv}
                    --target-env=${GLSLC_ENV}
                    $<$<BOOL:${arg_STAGE}>:-fshader-stage=${arg_STAGE}>
                    ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        )

        cmake_path(REPLACE_EXTENSION spv LAST_ONLY h OUTPUT_VARIABLE emb)
        semb_generate(${target} OUT ${emb} BINARY FILES ${spv})
    endforeach()
endfunction()

# the library
add_subdirectory(lib)

# executable
add_executable(nbody main.c renderer.c)
target_link_libraries(nbody PRIVATE nbody-lib glfw)
target_compile_options(nbody PRIVATE ${nbody_compiler_flags})

# benchmark uses clock_gettime which is not available on Windows and MacOS
if (UNIX AND NOT APPLE)
    add_executable(nbody-bench bench.c)
    target_link_libraries(nbody-bench PRIVATE nbody-lib)
    target_compile_options(nbody-bench PRIVATE ${nbody_compiler_flags})
endif()
