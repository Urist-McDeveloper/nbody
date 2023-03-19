find_library(raylib raylib)
if (NOT raylib)
    set(OpenGL_GL_PREFERENCE GLVND)
    add_subdirectory(raylib)
else()
    message(STATUS "Using system raylib")
endif()

add_subdirectory(semb)

add_library(acutest INTERFACE)
target_include_directories(acutest INTERFACE acutest)
