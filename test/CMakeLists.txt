# a hundred targets CTest adds by default are annoying
set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
include(CTest)

function(test_from FILE)
    # use file name without extension as target name
    get_filename_component(target ${FILE} NAME_WE)

    add_executable(${target} ${FILE})
    set_property(TARGET ${target} PROPERTY C_EXTENSIONS off)

    add_test(NAME ${target} COMMAND ${target})
    target_link_libraries(${target} acutest ${ARGN})
endfunction()

test_from(test_particle_sort.c)
