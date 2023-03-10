find_library(REQUIRED m)
find_package(OpenMP REQUIRED)

set(rag_lib_headers
        ${CMAKE_SOURCE_DIR}/include/rag.h
        ${CMAKE_SOURCE_DIR}/include/rag_vk.h)
set(rag_lib_sources
        body.c
        fio.c
        quadtree.c
        vk.c
        world.c)

add_library(rag-lib STATIC ${rag_lib_sources} ${rag_lib_headers})
target_include_directories(rag-lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(rag-lib PUBLIC OpenMP::OpenMP_C m)

if (MSVC)
    target_compile_options(rag-lib PUBLIC /Wall)
else()
    target_compile_options(rag-lib PUBLIC -Wall -Wextra -Werror)
endif()

set_target_properties(rag-lib PROPERTIES
        C_EXTENSIONS off
        OUTPUT_NAME rag)
