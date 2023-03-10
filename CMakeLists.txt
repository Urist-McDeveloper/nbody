cmake_minimum_required(VERSION 3.20)
project(rag C)

##
## General configuration
##

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

# enable IPO on Release (if supported)
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if (ipo_supported)
    set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif ()

##
## "Global" functions
##

find_package(Vulkan COMPONENTS glslc)
find_program(GLSLC NAMES glslc HINTS Vulkan::glslc)

set(GLSLC_ENV "vulkan1.0" CACHE STRING "glslc --target-env=")

function(compile_shaders target stage)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "STAGE;DEPENDS" "SOURCE")
    foreach(source ${arg_SOURCE})
        cmake_path(REPLACE_EXTENSION source LAST_ONLY spv OUTPUT_VARIABLE spv)
        add_custom_command(
                OUTPUT ${spv}
                DEPENDS ${source} ${arg_DEPENDS}
                COMMAND
                ${GLSLC} -o ${spv}
                --target-env=${GLSLC_ENV}
                $<$<BOOL:${arg_STAGE}>:-fshader-stage=${arg_STAGE}>
                ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        )
        target_sources(${target} PRIVATE ${spv})
    endforeach()
endfunction()

##
## Subdirectories
##

add_subdirectory(src)
add_subdirectory(deps)
add_subdirectory(test)
