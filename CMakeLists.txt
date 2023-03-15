cmake_minimum_required(VERSION 3.20)
project(nbody C)

##
##  General configuration
##

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(USE_AVX "Use AVX for CPU simulation" ON)

##
##  "Global" variables and functions
##

if (MSVC)
    set(nbody_compiler_flags /W4 /WX)
else()
    set(nbody_compiler_flags -Wall -Wextra -Werror)
endif()

find_package(Vulkan COMPONENTS glslc)
find_program(GLSLC NAMES glslc HINTS Vulkan::glslc)

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
        target_sources(${target} PRIVATE ${spv})
    endforeach()
endfunction()

##
##  Subdirectories
##

add_subdirectory(src)
add_subdirectory(deps)
add_subdirectory(test)
